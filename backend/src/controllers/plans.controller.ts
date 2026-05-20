import { Request, Response } from 'express';
import { z } from 'zod';
import { Plan, Device } from '../models/schemas';
import { logger } from '../utils/logger';
import { getMqttClient } from '../services/mqtt.service';
import { PlanRule } from '../models/types';

/* ── Validation schemas ──────────────────────────────────────────────────── */

const sliceSchema = z.object({
  start_time:    z.string().regex(/^\d{2}:\d{2}$/),
  stop_time:     z.string().regex(/^\d{2}:\d{2}$/),
  target_liters: z.number().positive().optional(),   // null/absent = time-only
});

const ruleSchema = z.object({
  day_of_week: z.enum(['monday','tuesday','wednesday','thursday','friday','saturday','sunday']),
  slices: z.array(sliceSchema).min(1),
});

const planSchema = z.object({
  name:       z.string().min(1),
  date_start: z.string().regex(/^\d{4}-\d{2}-\d{2}$/),
  date_stop:  z.string().regex(/^\d{4}-\d{2}-\d{2}$/).optional(),
  timezone:   z.string().default('Africa/Tunis'),
  rules:      z.array(ruleSchema).min(1),
});

/* ── Overlap helpers ─────────────────────────────────────────────────────── */

type Slice = { start_time: string; stop_time: string };
type Rule  = { day_of_week: string; slices: Slice[] };

/** True if two "HH:MM" ranges [a,b) and [c,d) overlap. */
function timesOverlap(a: string, b: string, c: string, d: string): boolean {
  return a < d && c < b;
}

/** True if two date ranges overlap (null stop = open-ended / forever). */
function datesOverlap(
  s1: string, e1: string | null | undefined,
  s2: string, e2: string | null | undefined,
): boolean {
  const end1 = e1 ?? '9999-12-31';
  const end2 = e2 ?? '9999-12-31';
  return s1 <= end2 && s2 <= end1;
}

/**
 * Returns the first conflict description inside a single plan's rules,
 * or null if none.
 */
function intraConflict(rules: Rule[]): string | null {
  for (const rule of rules) {
    const slices = rule.slices;
    for (let i = 0; i < slices.length; i++) {
      for (let j = i + 1; j < slices.length; j++) {
        if (timesOverlap(slices[i].start_time, slices[i].stop_time,
                         slices[j].start_time, slices[j].stop_time)) {
          return `Slots ${slices[i].start_time}–${slices[i].stop_time} and `
               + `${slices[j].start_time}–${slices[j].stop_time} overlap `
               + `on ${rule.day_of_week}`;
        }
      }
    }
  }
  return null;
}

/**
 * Returns the first conflict description between the new plan and an
 * existing plan, or null if none.
 */
function interConflict(
  newRules:      Rule[],
  newDateStart:  string,
  newDateStop:   string | null | undefined,
  existing:      { name: string; date_start: string; date_stop?: string | null; rules: unknown[] }[],
): string | null {
  for (const other of existing) {
    if (!datesOverlap(newDateStart, newDateStop, other.date_start, other.date_stop)) continue;

    const otherRules = other.rules as Rule[];
    for (const newRule of newRules) {
      const otherRule = otherRules.find(r => r.day_of_week === newRule.day_of_week);
      if (!otherRule) continue;

      for (const ns of newRule.slices) {
        for (const os of otherRule.slices) {
          if (timesOverlap(ns.start_time, ns.stop_time, os.start_time, os.stop_time)) {
            return `Conflicts with plan "${other.name}" on `
                 + `${newRule.day_of_week} at ${ns.start_time}–${ns.stop_time}`;
          }
        }
      }
    }
  }
  return null;
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/**
 * Return the current UTC offset in whole minutes for an IANA timezone.
 * Uses Intl.DateTimeFormat — no extra packages needed.
 */
function getUtcOffsetMinutes(timezone: string): number {
  try {
    const parts = new Intl.DateTimeFormat('en-US', {
      timeZone: timezone,
      timeZoneName: 'longOffset',
    }).formatToParts(new Date());
    const tzStr = parts.find(p => p.type === 'timeZoneName')?.value ?? '';
    const m = tzStr.match(/GMT([+-])(\d{2}):(\d{2})/);
    if (!m) return 0;
    const sign = m[1] === '+' ? 1 : -1;
    return sign * (parseInt(m[2], 10) * 60 + parseInt(m[3], 10));
  } catch {
    return 0;
  }
}

/** Minimal plan shape accepted by buildDevicePayload */
export interface PlanForDevice {
  name:        string;
  date_start:  string;
  date_stop?:  string | null;
  timezone:    string;
  rules:       PlanRule[];
}

/** Build the MQTT set_plan payload for the water valve firmware. */
export function buildDevicePayload(
  planId:   string,
  planName: string,
  enabled:  boolean,
  plan:     PlanForDevice,
): object {
  const rules = plan.rules.map(rule => ({
    day_of_week: rule.day_of_week,
    slices: rule.slices.map(slice => ({
      start_time:    slice.start_time,
      stop_time:     slice.stop_time,
      ...(slice.target_liters ? { target_liters: slice.target_liters } : {}),
    })),
  }));

  return {
    cmd: 'set_plan',
    plan: {
      id:         planId,
      name:       planName,
      enabled,
      date_start: plan.date_start,
      date_stop:  plan.date_stop ?? null,
      rules,
    },
  };
}

async function publishPlanToDevice(
  deviceStrId: string,
  planId:      string,
  planName:    string,
  enabled:     boolean,
  plan:        z.infer<typeof planSchema>,
): Promise<void> {
  const client = getMqttClient();
  if (!client) {
    logger.warn('publishPlanToDevice: MQTT not connected', { device: deviceStrId });
    return;
  }

  const payload = buildDevicePayload(planId, planName, enabled, plan);
  const payloadStr = JSON.stringify(payload);

  client.publish(`WATER/${deviceStrId}/cmd`, payloadStr, { qos: 1, retain: false });
  logger.info('Plan sent to device', { device: deviceStrId, planId, enabled, bytes: payloadStr.length });
}

function publishDeletePlanToDevice(deviceStrId: string, planId: string): void {
  const client = getMqttClient();
  if (!client) return;
  client.publish(
    `WATER/${deviceStrId}/cmd`,
    JSON.stringify({ cmd: 'delete_plan', plan_id: planId }),
    { qos: 1, retain: false },
  );
  logger.info('Plan delete sent to device', { device: deviceStrId, planId });
}

/* ── Sync helper (shared by syncPlansToDevice) ───────────────────────────── */

export async function pushAllPlansForDevice(deviceStrId: string): Promise<number> {
  const client = getMqttClient();
  if (!client) return 0;

  const deviceDoc = await Device.findOne({ device_id: deviceStrId }).select('_id').lean();
  const plans = await Plan.find({ device_id: deviceDoc?._id }).lean();

  if (plans.length === 0) return 0;

  for (const plan of plans) {
    const planId = (plan as unknown as { _id: { toString(): string } })._id.toString();
    const payload = buildDevicePayload(planId, plan.name, plan.enabled, {
      name:       plan.name,
      date_start: plan.date_start,
      date_stop:  plan.date_stop ?? undefined,
      timezone:   plan.timezone,
      rules:      plan.rules as z.infer<typeof ruleSchema>[],
    });
    client.publish(`WATER/${deviceStrId}/cmd`, JSON.stringify(payload), { qos: 1, retain: false });
  }

  logger.info('All plans pushed to device', { device: deviceStrId, count: plans.length });
  return plans.length;
}

/* ── Route handlers ──────────────────────────────────────────────────────── */

export async function listPlans(req: Request, res: Response): Promise<void> {
  const { deviceId } = req.params;
  try {
    const plans = await Plan.find({ device_id: deviceId }).sort({ created_at: -1 });
    res.json({ plans });
  } catch (err) {
    logger.error('listPlans error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function createPlan(req: Request, res: Response): Promise<void> {
  const { deviceId } = req.params;
  const parsed = planSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  try {
    const device = await Device.findById(deviceId).select('device_id').lean();
    if (!device) { res.status(404).json({ error: 'Device not found' }); return; }

    const { name, date_start, date_stop, timezone, rules } = parsed.data;

    // ── Intra-plan slice overlap check ────────────────────────────────────
    const intra = intraConflict(rules as Rule[]);
    if (intra) { res.status(409).json({ error: intra }); return; }

    // ── Inter-plan conflict check ─────────────────────────────────────────
    const siblings = await Plan.find({ device_id: deviceId }).select('name date_start date_stop rules').lean();
    const inter = interConflict(rules as Rule[], date_start, date_stop, siblings as never[]);
    if (inter) { res.status(409).json({ error: inter }); return; }

    const plan = await Plan.create({
      device_id: deviceId,
      name,
      date_start,
      date_stop: date_stop ?? null,
      timezone,
      rules,
      created_by: req.user!.userId,
    });

    // Auto-publish to device immediately
    const planId = (plan as unknown as { _id: { toString(): string } })._id.toString();
    await publishPlanToDevice(
      (device as unknown as { device_id: string }).device_id,
      planId, plan.name, plan.enabled,
      { name, date_start, date_stop, timezone, rules },
    );

    res.status(201).json({ plan });
  } catch (err) {
    logger.error('createPlan error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function updatePlan(req: Request, res: Response): Promise<void> {
  const { id } = req.params;
  const parsed = planSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  const { name, date_start, date_stop, timezone, rules } = parsed.data;

  try {
    // ── Intra-plan slice overlap check ────────────────────────────────────
    const intra = intraConflict(rules as Rule[]);
    if (intra) { res.status(409).json({ error: intra }); return; }

    // ── Inter-plan conflict check (exclude self) ───────────────────────────
    const current = await Plan.findById(id).select('device_id').lean();
    if (!current) { res.status(404).json({ error: 'Plan not found' }); return; }
    const siblings = await Plan.find({ device_id: current.device_id, _id: { $ne: id } })
      .select('name date_start date_stop rules').lean();
    const inter = interConflict(rules as Rule[], date_start, date_stop, siblings as never[]);
    if (inter) { res.status(409).json({ error: inter }); return; }

    const plan = await Plan.findByIdAndUpdate(
      id,
      { name, date_start, date_stop: date_stop ?? null, timezone, rules },
      { new: true },
    ).populate<{ device_id: { device_id: string } }>(
      'device_id', 'device_id',
    );

    if (!plan) { res.status(404).json({ error: 'Plan not found' }); return; }

    // Auto-publish to device immediately
    const dev = plan.device_id as unknown as { device_id: string };
    await publishPlanToDevice(
      dev.device_id,
      plan.id as string, plan.name, plan.enabled,
      { name, date_start, date_stop, timezone, rules },
    );

    res.json({ plan });
  } catch (err) {
    logger.error('updatePlan error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function deletePlan(req: Request, res: Response): Promise<void> {
  const { id } = req.params;

  try {
    const plan = await Plan.findById(id)
      .populate<{ device_id: { device_id: string } }>('device_id', 'device_id');

    if (!plan) { res.status(404).json({ error: 'Plan not found' }); return; }

    publishDeletePlanToDevice(
      (plan.device_id as unknown as { device_id: string }).device_id,
      plan.id as string,
    );

    await plan.deleteOne();
    res.json({ message: 'Plan deleted' });
  } catch (err) {
    logger.error('deletePlan error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function togglePlan(req: Request, res: Response): Promise<void> {
  const { id } = req.params;

  try {
    const plan = await Plan.findById(id)
      .populate<{ device_id: { device_id: string } }>('device_id', 'device_id');

    if (!plan) { res.status(404).json({ error: 'Plan not found' }); return; }

    plan.enabled = !plan.enabled;
    await plan.save();

    const dev = plan.device_id as unknown as { device_id: string };

    await publishPlanToDevice(
      dev.device_id,
      plan.id as string, plan.name, plan.enabled,
      {
        name:       plan.name,
        date_start: plan.date_start,
        date_stop:  plan.date_stop ?? undefined,
        timezone:   plan.timezone,
        rules:      plan.rules as z.infer<typeof ruleSchema>[],
      },
    );

    res.json({ plan });
  } catch (err) {
    logger.error('togglePlan error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

/**
 * POST /devices/:deviceId/plans/sync
 *
 * Re-publishes every plan for this device to MQTT with freshly resolved IR
 * codes.  One tap in the app → all plans land on the device immediately.
 * Useful after:
 *  - the device reconnected after being offline when plans were created
 *  - the IR code was just assigned / changed
 *  - the user manually wants to force a full sync
 */
export async function syncPlansToDevice(req: Request, res: Response): Promise<void> {
  const { deviceId } = req.params;
  try {
    const device = await Device.findById(deviceId).select('device_id').lean();
    if (!device) { res.status(404).json({ error: 'Device not found' }); return; }

    if (!getMqttClient()) {
      res.status(503).json({ error: 'MQTT broker not connected' });
      return;
    }

    const plans = await Plan.find({ device_id: deviceId }).lean();
    if (plans.length === 0) {
      res.json({ message: 'No plans to sync', count: 0 });
      return;
    }

    const client = getMqttClient()!;
    for (const plan of plans) {
      const planId = (plan as unknown as { _id: { toString(): string } })._id.toString();
      const payload = buildDevicePayload(planId, plan.name, plan.enabled, {
        name:       plan.name,
        date_start: plan.date_start,
        date_stop:  plan.date_stop ?? undefined,
        timezone:   plan.timezone,
        rules:      plan.rules as z.infer<typeof ruleSchema>[],
      });
      client.publish(`WATER/${device.device_id}/cmd`, JSON.stringify(payload), { qos: 1, retain: false });
    }

    logger.info('Plans synced to device', { device: device.device_id, count: plans.length });
    res.json({ message: `${plans.length} plan(s) sent to device`, count: plans.length });
  } catch (err) {
    logger.error('syncPlansToDevice error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}
