#include "BikeHeaders.h"
#include "Framework/MathLib.h"

// Wind state accessed via g_wind (defined in BikeWind.cpp)

// ------------------------------------------------------------
// BikeObject::tick_stamina
//
// Updates the rider's physiology each tick: heat stress, FTP, W', glycogen,
// lactate, and heart rate. Clamps and slews ci.power to the current ceiling.
// ------------------------------------------------------------
void BikeObject::tick_stamina(ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	StaminaState& s = stamina;
	const RiderStats& r = rider;

	// Heat stress: solar + ambient + effort heat vs. convective cooling from airspeed
	{
		const float temp_above_neutral = glm::max(0.f, g_wind.ambient_temp - 18.f);
		const float solar_heat   = g_wind.sun_exposure * 0.00025f;
		const float ambient_heat = temp_above_neutral * 0.000015f;
		const float effort_heat  = s.actual_power * 0.0000005f;
		const float air_flow     = glm::max(1.f, speed - get_wind_along_bike());
		const float cooling      = air_flow * 0.000035f;
		s.heat_stress += (solar_heat + ambient_heat + effort_heat - cooling) * dt;
		s.heat_stress  = glm::clamp(s.heat_stress, 0.f, 1.f);
	}

	// Effective FTP and power ceiling
	const float glycogen_factor = 0.55f + 0.45f * s.glycogen;
	const float heat_ftp_factor = 1.f - s.heat_stress * 0.12f;
	s.effective_ftp  = r.base_ftp * glycogen_factor * heat_ftp_factor;
	s.power_ceiling  = s.effective_ftp + (r.sprint_watts - s.effective_ftp) * (s.w_prime / r.w_prime_max);
	s.actual_power   = glm::min(ci.power, glm::max(0.f, s.power_ceiling));
	ci.power         = s.actual_power;

	// W' drain / recovery
	const float recovery_pct = 0.85f;
	if (s.actual_power > s.effective_ftp) {
		s.w_prime = glm::max(0.f, s.w_prime - (s.actual_power - s.effective_ftp) * dt);
	} else if (s.actual_power < s.effective_ftp * recovery_pct) {
		const float zone2_factor = 1.f - s.actual_power / (s.effective_ftp * recovery_pct);
		s.w_prime = glm::min(r.w_prime_max, s.w_prime + 22.f * zone2_factor * dt);
	}

	// Glycogen drain
	if (s.actual_power > 0.f) {
		const float ratio     = s.actual_power / s.effective_ftp;
		const float heat_mult = 1.f + s.heat_stress * 0.30f;
		s.glycogen = glm::max(0.f, s.glycogen - 0.0000833f * glm::pow(ratio, 1.5f) * heat_mult * dt);
	}

	// Lactate (O2-debt): accumulates above FTP, decays tau~5min
	if (s.actual_power > s.effective_ftp)
		s.lactate += (s.actual_power - s.effective_ftp) * dt;
	s.lactate = glm::min(20000.f, damp_dt_independent(0.f, s.lactate, 0.9967f, dt));
	const float lactate_hr = s.lactate * 0.002f;

	// Heart rate
	const float power_frac          = glm::clamp(s.actual_power / (s.effective_ftp * 1.15f), 0.f, 1.f);
	const float hr_target_from_power = r.hr_rest + (r.hr_max - r.hr_rest) * power_frac + s.heat_stress * 20.f;
	const float drift_target         = (1.f - s.glycogen) * 18.f * (1.f + s.heat_stress);
	s.hr_drift = damp_dt_independent(drift_target, s.hr_drift, 0.9917f, dt);
	const float hr_target = glm::max(hr_target_from_power, r.hr_rest + lactate_hr) + s.hr_drift;
	const float tau_coeff = (s.hr_current < hr_target) ? exp(-1/30.0) : exp(-1/60.0);
	s.hr_current = glm::clamp(damp_dt_independent(hr_target, s.hr_current, tau_coeff, dt), r.hr_rest, r.hr_max + 5.f);

	// HR pulse phase
	s.hr_pulse_phase += (s.hr_current / 60.f) * 6.2832f * dt;
	if (s.hr_pulse_phase > 6.2832f) s.hr_pulse_phase -= 6.2832f;
}
