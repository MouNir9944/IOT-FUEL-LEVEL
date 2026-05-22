import mongoose, { Schema, Document, Types } from 'mongoose';

const jsonTransform = (_: unknown, ret: Record<string, unknown>) => {
  ret.id = (ret._id as Types.ObjectId)?.toString();
  delete ret._id;
  delete ret.__v;
  return ret;
};

// ─── User ────────────────────────────────────────────────────────────────────

export interface IUser extends Document {
  email: string;
  password_hash: string;
  role: 'superadmin' | 'admin' | 'user' | 'technician';
  full_name: string;
  active: boolean;
  is_primary_admin: boolean;
  created_by_admin_id: Types.ObjectId | null;
  created_at: Date;
  updated_at: Date;
}

const UserSchema = new Schema<IUser>(
  {
    email: { type: String, required: true, unique: true, lowercase: true, trim: true },
    password_hash: { type: String, required: true, select: false },
    role: { type: String, enum: ['superadmin', 'admin', 'user', 'technician'], required: true },
    full_name: { type: String, default: '' },
    active: { type: Boolean, default: true },
    is_primary_admin: { type: Boolean, default: false },
    created_by_admin_id: { type: Schema.Types.ObjectId, ref: 'User', default: null },
  },
  {
    timestamps: { createdAt: 'created_at', updatedAt: 'updated_at' },
    toJSON: {
      virtuals: true,
      transform: (doc, ret) => {
        const r = ret as Record<string, unknown>;
        jsonTransform(doc, r);
        delete r.password_hash;
        return r;
      },
    },
  },
);

export const User = mongoose.model<IUser>('User', UserSchema);

// ─── RefreshToken ─────────────────────────────────────────────────────────────

export interface IRefreshToken extends Document {
  user_id: Types.ObjectId;
  token_hash: string;
  expires_at: Date;
  created_at: Date;
}

const RefreshTokenSchema = new Schema<IRefreshToken>(
  {
    user_id: { type: Schema.Types.ObjectId, ref: 'User', required: true },
    token_hash: { type: String, required: true, unique: true },
    expires_at: { type: Date, required: true },
  },
  { timestamps: { createdAt: 'created_at', updatedAt: false } },
);

RefreshTokenSchema.index({ expires_at: 1 }, { expireAfterSeconds: 0 });

export const RefreshToken = mongoose.model<IRefreshToken>('RefreshToken', RefreshTokenSchema);

// ─── Site ─────────────────────────────────────────────────────────────────────

export interface ISite extends Document {
  name: string;
  address: string | null;
  admin_id: Types.ObjectId;
  created_at: Date;
  updated_at: Date;
}

const SiteSchema = new Schema<ISite>(
  {
    name: { type: String, required: true, trim: true },
    address: { type: String, default: null },
    admin_id: { type: Schema.Types.ObjectId, ref: 'User', required: true },
  },
  {
    timestamps: { createdAt: 'created_at', updatedAt: 'updated_at' },
    toJSON: { virtuals: true, transform: jsonTransform },
  },
);

export const Site = mongoose.model<ISite>('Site', SiteSchema);

// ─── Device ───────────────────────────────────────────────────────────────────

export interface IDevice extends Document {
  device_id: string;
  name: string;
  site_id: Types.ObjectId;
  qr_data: string | null;
  mqtt_broker: string | null;
  topics: Record<string, string> | null;
  last_status: string;
  last_status_at: Date | null;
  last_lwt_at: Date | null;
  last_lwt_payload: Record<string, unknown> | null;
  last_telemetry_at: Date | null;
  last_telemetry: Record<string, unknown> | null;
  /** Last firmware config successfully sent through the app (firmware format, metres). */
  last_config: Record<string, unknown> | null;
  alert_on_offline: boolean;
  /** Firmware/software version reported by the device (e.g. "1.0.0") */
  sw_version: string | null;
  /** Hardware revision reported by the device (e.g. "v1") */
  hw_version: string | null;
  created_at: Date;
  updated_at: Date;
}

const DeviceSchema = new Schema<IDevice>(
  {
    device_id: { type: String, required: true, unique: true, trim: true },
    name: { type: String, default: '' },
    site_id: { type: Schema.Types.ObjectId, ref: 'Site', required: true },

    qr_data: { type: String, default: null },
    mqtt_broker: { type: String, default: null },
    topics: { type: Schema.Types.Mixed, default: null },
    last_status: { type: String, default: 'unknown' },
    last_status_at: { type: Date, default: null },
    last_lwt_at: { type: Date, default: null },
    last_lwt_payload: { type: Schema.Types.Mixed, default: null },
    last_telemetry_at: { type: Date, default: null },
    last_telemetry: { type: Schema.Types.Mixed, default: null },
    last_config:    { type: Schema.Types.Mixed, default: null },
    alert_on_offline: { type: Boolean, default: true },
    sw_version: { type: String, default: null },
    hw_version: { type: String, default: null },
  },
  {
    timestamps: { createdAt: 'created_at', updatedAt: 'updated_at' },
    toJSON: { virtuals: true, transform: jsonTransform },
  },
);

export const Device = mongoose.model<IDevice>('Device', DeviceSchema);

// ─── UserSite ─────────────────────────────────────────────────────────────────

export interface IUserSite extends Document {
  user_id: Types.ObjectId;
  site_id: Types.ObjectId;
  device_ids: Types.ObjectId[];
  created_at: Date;
}

const UserSiteSchema = new Schema<IUserSite>(
  {
    user_id: { type: Schema.Types.ObjectId, ref: 'User', required: true },
    site_id: { type: Schema.Types.ObjectId, ref: 'Site', required: true },
    device_ids: [{ type: Schema.Types.ObjectId, ref: 'Device' }],
  },
  {
    timestamps: { createdAt: 'created_at', updatedAt: false },
    toJSON: { virtuals: true, transform: jsonTransform },
  },
);

UserSiteSchema.index({ user_id: 1, site_id: 1 }, { unique: true });

export const UserSite = mongoose.model<IUserSite>('UserSite', UserSiteSchema);

// ─── Plan ─────────────────────────────────────────────────────────────────────

export interface IPlan extends Document {
  device_id: Types.ObjectId;
  name: string;
  date_start: string;
  date_stop: string | null;
  timezone: string;
  rules: unknown[];
  enabled: boolean;
  created_by: Types.ObjectId | null;
  created_at: Date;
  updated_at: Date;
}

const PlanSchema = new Schema<IPlan>(
  {
    device_id: { type: Schema.Types.ObjectId, ref: 'Device', required: true },
    name: { type: String, required: true },
    date_start: { type: String, required: true },
    date_stop: { type: String, default: null },
    timezone: { type: String, default: 'Africa/Tunis' },
    rules: { type: [Schema.Types.Mixed], default: [] },
    enabled: { type: Boolean, default: true },
    created_by: { type: Schema.Types.ObjectId, ref: 'User', default: null },
  },
  {
    timestamps: { createdAt: 'created_at', updatedAt: 'updated_at' },
    toJSON: { virtuals: true, transform: jsonTransform },
  },
);

export const Plan = mongoose.model<IPlan>('Plan', PlanSchema);

// ─── DeviceConnectionLog ──────────────────────────────────────────────────────

export interface IDeviceConnectionLog extends Document {
  device_id: Types.ObjectId;
  status: 'online' | 'offline';
  reason: string | null;
  payload: Record<string, unknown> | null;
  created_at: Date;
}

const DeviceConnectionLogSchema = new Schema<IDeviceConnectionLog>(
  {
    device_id: { type: Schema.Types.ObjectId, ref: 'Device', required: true },
    status: { type: String, enum: ['online', 'offline'], required: true },
    reason: { type: String, default: null },
    payload: { type: Schema.Types.Mixed, default: null },
  },
  {
    timestamps: { createdAt: 'created_at', updatedAt: false },
    toJSON: { virtuals: true, transform: jsonTransform },
  },
);

export const DeviceConnectionLog = mongoose.model<IDeviceConnectionLog>(
  'DeviceConnectionLog',
  DeviceConnectionLogSchema,
);

// ─── CommandLog ───────────────────────────────────────────────────────────────

export interface ICommandLog extends Document {
  device_id: Types.ObjectId | null;
  command: Record<string, unknown>;
  source_user_id: Types.ObjectId | null;
  source_type: 'manual' | 'planning' | 'auto' | null;
  created_at: Date;
}

const CommandLogSchema = new Schema<ICommandLog>(
  {
    device_id: { type: Schema.Types.ObjectId, ref: 'Device', default: null },
    command: { type: Schema.Types.Mixed, required: true },
    source_user_id: { type: Schema.Types.ObjectId, ref: 'User', default: null },
    source_type: { type: String, enum: ['manual', 'planning', 'auto', null], default: null },
  },
  {
    timestamps: { createdAt: 'created_at', updatedAt: false },
    toJSON: { virtuals: true, transform: jsonTransform },
  },
);

export const CommandLog = mongoose.model<ICommandLog>('CommandLog', CommandLogSchema);

