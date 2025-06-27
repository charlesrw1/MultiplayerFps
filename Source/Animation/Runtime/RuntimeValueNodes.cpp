
bool atLogicalOpNode::get_bool(atUpdateStack& ctx) const {
	if (is_and) {
		bool res = true;
		for (auto p : ptrs) {
			bool b = p->get_bool(ctx);
			if (!b) {
				res = false;
				break;
			}
		}
		return res;
	}
	else {
		bool res = false;
		for (auto p : ptrs) {
			bool b = p->get_bool(ctx);
			if (b) {
				res = true;
				break;
			}
		}
		return res;
	}
}

bool atMathNode::get_bool(atUpdateStack& ctx) const {
	auto optype = get_optype();
	auto intype = atValueNode::Type(inputTypeInt);
	switch (intype)
	{
	case atValueNode::Float:
		switch (optype)
		{
		case MathNodeType::Lt: return lptr->get_float(ctx) < rptr->get_float(ctx);
		case MathNodeType::Gt:  return lptr->get_float(ctx) > rptr->get_float(ctx);
		case MathNodeType::Leq: return lptr->get_float(ctx) <= rptr->get_float(ctx);
		case MathNodeType::Geq: return lptr->get_float(ctx) >= rptr->get_float(ctx);
			break;
		default:
			break;
		}
		break;
	case atValueNode::Bool:
		switch (optype) {
		case MathNodeType::Eq:  return lptr->get_bool(ctx) == rptr->get_bool(ctx);
		case MathNodeType::Neq: return lptr->get_bool(ctx) != rptr->get_bool(ctx);
		default:
			break;
		}
		break;
	case atValueNode::Int:
		switch (optype) {
		case MathNodeType::Lt: return lptr->get_int(ctx) < rptr->get_int(ctx);
		case MathNodeType::Gt:  return lptr->get_int(ctx) > rptr->get_int(ctx);
		case MathNodeType::Leq: return lptr->get_int(ctx) <= rptr->get_int(ctx);
		case MathNodeType::Geq: return lptr->get_int(ctx) >= rptr->get_int(ctx);
		case MathNodeType::Eq: return lptr->get_int(ctx) == rptr->get_int(ctx);
		case MathNodeType::Neq: return lptr->get_int(ctx) != rptr->get_int(ctx);
		default:
			break;
		}
		break;
	default:
		break;
	}

	sys_print(Error, "not defined\n");
	return false;
}

float atMathNode::get_float(atUpdateStack& ctx) const {
	auto optype = get_optype();
	switch (optype)
	{
	case MathNodeType::Add: return lptr->get_float(ctx) + rptr->get_float(ctx);
	case MathNodeType::Sub: return lptr->get_float(ctx) - rptr->get_float(ctx);
	case MathNodeType::Mult: return lptr->get_float(ctx) * rptr->get_float(ctx);
	case MathNodeType::Div: return lptr->get_float(ctx) / rptr->get_float(ctx);
	default:
		break;
	}
	return 0.f;
}

int atMathNode::get_int(atUpdateStack& ctx) const {
	auto optype = get_optype();
	switch (optype)
	{
	case MathNodeType::Add: return lptr->get_int(ctx) + rptr->get_int(ctx);
	case MathNodeType::Sub: return lptr->get_int(ctx) - rptr->get_int(ctx);
	case MathNodeType::Mult: return lptr->get_int(ctx) * rptr->get_int(ctx);
	case MathNodeType::Div: return lptr->get_int(ctx) / rptr->get_int(ctx);
	default:
		break;
	}
	return 0;
}

vec3 atMathNode::get_vector3(atUpdateStack& ctx) const {
	auto optype = get_optype();
	switch (optype)
	{
	case MathNodeType::Add: return lptr->get_vector3(ctx) + rptr->get_vector3(ctx);
	case MathNodeType::Sub: return lptr->get_vector3(ctx) - rptr->get_vector3(ctx);
	case MathNodeType::Mult: return lptr->get_vector3(ctx) * rptr->get_vector3(ctx);
	case MathNodeType::Div: return lptr->get_vector3(ctx) / rptr->get_vector3(ctx);
	default:
		break;
	}
	return vec3(0.f);
}

void atVariableNode::init(atInitContext& ctx) {
	for (auto prop : ClassPropPtr(&ctx.get_instance_type())) {
		if (prop.get_name() == varName) {
			this->p = prop.get_property_info();
			break;
		}
	}
	if (!p) {
		sys_print(Error, "couldnt find variable in graph: %s\n", varName.c_str());
	}
}

bool atVariableNode::get_bool(atUpdateStack& ctx) const {
	PropertyPtr ptr(p, &ctx.graph.obj);
	assert(ptr.is_boolean());
	return ptr.as_boolean();
}

float atVariableNode::get_float(atUpdateStack& ctx) const {
	PropertyPtr ptr(p, &ctx.graph.obj);
	assert(ptr.is_float());
	return ptr.as_float();
}

int atVariableNode::get_int(atUpdateStack& ctx) const {
	PropertyPtr ptr(p, &ctx.graph.obj);
	assert(ptr.is_numeric());
	return (int)ptr.get_integer_casted();
}

vec3 atVariableNode::get_vector3(atUpdateStack& ctx) const {
	PropertyPtr ptr(p, &ctx.graph.obj);
	assert(ptr.is_vec3());
	return ptr.as_vec3();
}
