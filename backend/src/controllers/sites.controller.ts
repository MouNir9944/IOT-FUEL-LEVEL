import { Request, Response } from 'express';
import { z } from 'zod';
import mongoose from 'mongoose';
import { Site, Device, UserSite } from '../models/schemas';
import { logger } from '../utils/logger';

const siteSchema = z.object({
  name: z.string().min(1),
  address: z.string().optional(),
});

export async function listSites(req: Request, res: Response): Promise<void> {
  const { userId, role } = req.user!;

  try {
    let sites: unknown[];

    if (role === 'superadmin') {
      sites = await Site.aggregate([
        {
          $lookup: {
            from: 'users',
            localField: 'admin_id',
            foreignField: '_id',
            as: 'admin',
          },
        },
        { $unwind: { path: '$admin', preserveNullAndEmptyArrays: true } },
        {
          $lookup: {
            from: 'devices',
            localField: '_id',
            foreignField: 'site_id',
            as: 'devices',
          },
        },
        {
          $project: {
            id: { $toString: '$_id' },
            name: 1,
            address: 1,
            admin_id: { $toString: '$admin_id' },
            admin_email: '$admin.email',
            admin_name: '$admin.full_name',
            device_count: { $size: '$devices' },
            created_at: 1,
          },
        },
        { $sort: { created_at: -1 } },
      ]);
    } else if (role === 'admin') {
      sites = await Site.aggregate([
        { $match: { admin_id: new mongoose.Types.ObjectId(userId) } },
        {
          $lookup: {
            from: 'devices',
            localField: '_id',
            foreignField: 'site_id',
            as: 'devices',
          },
        },
        {
          $project: {
            id: { $toString: '$_id' },
            name: 1,
            address: 1,
            admin_id: { $toString: '$admin_id' },
            device_count: { $size: '$devices' },
            created_at: 1,
          },
        },
        { $sort: { created_at: -1 } },
      ]);
    } else {
      // user / technician — only their assigned sites
      const userSites = await UserSite.find({ user_id: userId }).select('site_id').lean();
      const siteIds = userSites.map((us) => us.site_id);

      sites = await Site.aggregate([
        { $match: { _id: { $in: siteIds } } },
        {
          $lookup: {
            from: 'devices',
            localField: '_id',
            foreignField: 'site_id',
            as: 'devices',
          },
        },
        {
          $project: {
            id: { $toString: '$_id' },
            name: 1,
            address: 1,
            admin_id: { $toString: '$admin_id' },
            device_count: { $size: '$devices' },
            created_at: 1,
          },
        },
        { $sort: { created_at: -1 } },
      ]);
    }

    res.json({ sites });
  } catch (err) {
    logger.error('listSites error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function createSite(req: Request, res: Response): Promise<void> {
  const parsed = siteSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  const { name, address } = parsed.data;
  const adminId = req.user!.userId;

  try {
    const site = await Site.create({ name, address: address ?? null, admin_id: adminId });
    res.status(201).json({ site });
  } catch (err) {
    logger.error('createSite error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function updateSite(req: Request, res: Response): Promise<void> {
  const parsed = siteSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  const { id } = req.params;
  const { name, address } = parsed.data;
  const adminId = req.user!.userId;

  try {
    const site = await Site.findOneAndUpdate(
      { _id: id, admin_id: adminId },
      { name, address: address ?? null },
      { new: true },
    );

    if (!site) {
      res.status(404).json({ error: 'Site not found' });
      return;
    }
    res.json({ site });
  } catch (err) {
    logger.error('updateSite error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function deleteSite(req: Request, res: Response): Promise<void> {
  const { id } = req.params;
  const adminId = req.user!.userId;

  try {
    const result = await Site.findOneAndDelete({ _id: id, admin_id: adminId });

    if (!result) {
      res.status(404).json({ error: 'Site not found' });
      return;
    }

    // Cascade delete devices and user-site assignments
    await Device.deleteMany({ site_id: id });
    await UserSite.deleteMany({ site_id: id });

    res.json({ message: 'Site deleted' });
  } catch (err) {
    logger.error('deleteSite error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}
