#pragma once
#include "Game/EntityComponent.h"
#include "Framework/StructReflection.h"
#include "Animation/Runtime/Easing.h"
#include "Animation/Runtime/Percentage.h"
struct GraphCurve {
	STRUCT_BODY();
	REF void serialize(Serializer& s);
	float evalutate(Percentage percent) {
		return 0.0;
	}
	float evaluate(float time) {
		return 0.0;
	}
private:
	struct Points {
		float x = 0;
		float y = 0;
		Easing interp = Easing::Constant;
	};
	float end_time = 0.0;
	std::vector<Points> points;
};

class GraphCurveComponent : public Component {
public:
	CLASS_BODY(GraphCurveComponent);
	REF GraphCurve curve;
};