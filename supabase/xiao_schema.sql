-- XIAO ESP32-S3 remote dashboard: Supabase schema (final state of today's migrations)
-- Run in the Supabase SQL editor. Replace <DEVICE_SECRET> with a random hex string
-- and put the same value in SupabaseDashboard/secrets.h.

create schema if not exists private;
revoke all on schema private from anon, authenticated;

create table private.xiao_device_secret (
  device_id text primary key,
  secret text not null
);

create table public.xiao_state (
  device_id text primary key,
  led_on boolean not null default false,
  touched boolean not null default false,
  touch_value integer not null default 0,
  baseline integer not null default 0,
  local_ip text,
  device_seen_at timestamptz,
  updated_at timestamptz not null default now()
);

alter table public.xiao_state enable row level security;

create policy "xiao_state readable by everyone" on public.xiao_state
  for select to anon, authenticated using (true);
create policy "xiao_state led updatable by everyone" on public.xiao_state
  for update to anon, authenticated using (true) with check (true);

-- web clients may only change led_on
revoke insert, update, delete, truncate on public.xiao_state from anon, authenticated;
grant select on public.xiao_state to anon, authenticated;
grant update (led_on) on public.xiao_state to anon, authenticated;

create or replace function public.xiao_touch_updated_at()
returns trigger language plpgsql set search_path = '' as $$
begin
  new.updated_at := now();
  return new;
end $$;

create trigger xiao_state_updated_at before update on public.xiao_state
  for each row execute function public.xiao_touch_updated_at();

-- device report: checks secret, stores touch state + LAN IP, applies a LED change made on the
-- board's local dashboard (p_led), returns desired LED state
create function public.xiao_report(
  p_device text, p_secret text, p_touched boolean, p_value integer, p_baseline integer,
  p_ip text default null, p_led boolean default null
) returns boolean
language plpgsql security definer set search_path = '' as $$
declare
  v_led boolean;
begin
  if not exists (select 1 from private.xiao_device_secret s
                 where s.device_id = p_device and s.secret = p_secret) then
    raise exception 'invalid device credentials' using errcode = '28000';
  end if;

  if p_led is not null then
    update public.xiao_state set led_on = p_led
     where device_id = p_device and led_on is distinct from p_led;
  end if;

  -- write only on touch/IP change or every 2s (heartbeat) to limit realtime traffic
  update public.xiao_state
     set touched = p_touched, touch_value = p_value, baseline = p_baseline,
         local_ip = coalesce(p_ip, local_ip), device_seen_at = now()
   where device_id = p_device
     and (touched is distinct from p_touched
          or (p_ip is not null and local_ip is distinct from p_ip)
          or device_seen_at is null
          or device_seen_at < now() - interval '2 seconds');

  select led_on into v_led from public.xiao_state where device_id = p_device;
  return coalesce(v_led, false);
end $$;

revoke execute on function public.xiao_report(text, text, boolean, integer, integer, text, boolean) from public;
grant execute on function public.xiao_report(text, text, boolean, integer, integer, text, boolean) to anon, authenticated;
revoke execute on function public.xiao_touch_updated_at() from public, anon, authenticated;

insert into public.xiao_state (device_id) values ('xiao');
insert into private.xiao_device_secret (device_id, secret) values ('xiao', '<DEVICE_SECRET>');

alter publication supabase_realtime add table public.xiao_state;
