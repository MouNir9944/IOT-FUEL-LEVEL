/**
 * Seed script — creates the superadmin account if it doesn't exist.
 * Run: npm run seed
 */
import * as dotenv from 'dotenv';
dotenv.config();

import mongoose from 'mongoose';
import * as bcrypt from 'bcryptjs';

const MONGODB_URI = process.env.MONGODB_URI;
if (!MONGODB_URI) {
  console.error('MONGODB_URI is not set in .env');
  process.exit(1);
}

const UserSchema = new mongoose.Schema({
  email: { type: String, unique: true, lowercase: true, trim: true },
  password_hash: String,
  role: String,
  full_name: String,
  active: { type: Boolean, default: true },
  is_primary_admin: { type: Boolean, default: false },
  created_by_admin_id: { type: mongoose.Schema.Types.ObjectId, default: null },
}, { timestamps: { createdAt: 'created_at', updatedAt: 'updated_at' } });

const User = mongoose.model('User', UserSchema);

async function seed() {
  await mongoose.connect(MONGODB_URI!);
  console.log('Connected to MongoDB');

  const existing = await User.findOne({ email: 'superadmin@iotac.local' });
  if (existing) {
    console.log('Superadmin already exists — skipping.');
    return;
  }

  const password_hash = await bcrypt.hash('Admin@12345', 10);
  await User.create({
    email: 'superadmin@iotac.local',
    password_hash,
    role: 'superadmin',
    full_name: 'Super Admin',
    active: true,
    is_primary_admin: true,
  });

  console.log('Superadmin created:');
  console.log('  Email:    superadmin@iotac.local');
  console.log('  Password: Admin@12345');
  console.log('  Role:     superadmin');
}

seed()
  .catch((err) => {
    console.error('Seed failed:', err.message);
    process.exit(1);
  })
  .finally(() => mongoose.disconnect());
