#pragma once

namespace dalia {

	struct SlewFloat {
		float current = 0.0f;
		float target = 0.0f;
		float alpha = 0.05f;

		void SetTarget(float newTarget);
		void SnapToTarget();

		bool Process();
	};
}
