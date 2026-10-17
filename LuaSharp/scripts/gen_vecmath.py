#!/usr/bin/env python
"""Generates the LuaSharp native math types from one spec.

Outputs (both checked in; rerun after editing this file):
  runtime/luasharp/vecmath.h  C structs and functions for Vec2/Vec3/Vec2i/Vec3i/Quat/Mat4x4 and `math`
  src/BuiltinMath.inc         registry rows (members and operators) included by Builtins.cpp
Run with: py LuaSharp/scripts/gen_vecmath.py
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
CTYPE = {'int': 'ls_int', 'float': 'ls_float', 'bool': 'ls_bool', 'table': 'ls_table', 'void': 'void',
         'Vec2': 'LS_Vec2', 'Vec3': 'LS_Vec3', 'Vec2i': 'LS_Vec2i', 'Vec3i': 'LS_Vec3i', 'Quat': 'LS_Quat', 'Mat4x4': 'LS_Mat4x4'}

C = []        # C text after the struct definitions
ROWS = []     # BuiltinMember rows
OPS = []      # BuiltinOperator rows
USED = set()  # C names already taken
DOC = []      # (owner, name, ret, params, kind, static, defaults) for docs/math-library.md
DOC_OPS = []  # (left, op, right, result)


def cname_for(base):
    name, n = base, 1
    while name in USED:
        n += 1
        name = f'{base}_{n}'
    USED.add(name)
    return name


def func(cname, ret, params, body):
    ps = ', '.join(f'{CTYPE[t]} {n}' for t, n in params) or 'void'
    C.append(f'static {CTYPE[ret]} {cname}({ps}) {{ {body} }}')


def row(owner, name, ret, params, kind, static, cname, defaults):
    DOC.append((owner, name, ret, params, kind, static, defaults))
    ps = []
    for t, n in params:
        if n in defaults: ps.append(f'{{"{t}", "{n}", true, "{defaults[n]}"}}')
        else: ps.append(f'{{"{t}", "{n}"}}')
    ROWS.append(f'    {{"{owner}", "{name}", "{ret}", {{{", ".join(ps)}}}, SymbolKind::{kind}, {"true" if static else "false"}, Lowering::None, "{cname}"}},')


def member(owner, name, ret, params, body, static=False, defaults=None, cname=None):
    """Method (self first unless static). params excludes self; defaults maps parameter name -> C literal."""
    cname = cname_for(cname or f'ls_{owner}_{name}')
    func(cname, ret, ([] if static else [(owner, 'self')]) + params, body)
    row(owner, name, ret, params, 'Method', static, cname, defaults or {})


def field(owner, name, ret):
    DOC.append((owner, name, ret, [], 'Field', False, {}))
    ROWS.append(f'    {{"{owner}", "{name}", "{ret}", {{}}, SymbolKind::Field, false, Lowering::None, ""}},')


def constant(owner, name, ret, expr, body=None):
    """Static read-only value; expr is the C expression emitted at the use site."""
    if body:
        func(expr.removesuffix('()'), ret, [], body)
    DOC.append((owner, name, ret, [], 'Field', True, {}))
    ROWS.append(f'    {{"{owner}", "{name}", "{ret}", {{}}, SymbolKind::Field, true, Lowering::None, "{expr}"}},')


def operator(left, op, right, result, cname, body, params=None):
    cname = cname_for(cname)
    DOC_OPS.append((left, op, right, result))
    if right:
        func(cname, result, params or [(left, 'a'), (right, 'b')], body)
    else:
        func(cname, result, params or [(left, 'a')], body)
    OPS.append(f'    {{"{left}", "{op}", "{right}", "{result}", Lowering::None, "{cname}"}},')


def cw(comps, fmt):
    return ', '.join(fmt.format(c=c) for c in comps)


def total(comps, fmt):
    return ' + '.join(fmt.format(c=c) for c in comps)


# ---------------------------------------------------------------- vectors

def vector(name, comps, is_float):
    V, L = name, f'LS_{name}'
    S = 'float' if is_float else 'int'         # LuaSharp scalar type
    CS = 'float' if is_float else 'int'        # component C type
    Z = '0.0f' if is_float else '0'
    n = len(comps)
    for c in comps: field(V, c, S)
    ps = [(S, c) for c in comps]
    lit = lambda fmt: f'({L}){{ {cw(comps, fmt)} }}'
    cast = f'({CS})'

    member(V, 'new', V, ps, f'return {lit(cast + "{c}")};', static=True, defaults={c: '0' for c in comps})
    member(V, 'splat', V, [(S, 'value')], f'return {lit(cast + "value")};', static=True)
    constant(V, 'zero', V, f'ls_{V}_zero()', f'return {lit(Z)};')
    constant(V, 'one', V, f'ls_{V}_one()', f'return {lit("(" + CS + ")1")};')
    if n == 3:
        for i, c in enumerate(comps):
            constant(V, f'unit_{c}', V, f'ls_{V}_unit_{c}()', f'return ({L}){{ {", ".join("1" if j == i else "0" for j in range(n))} }};')

    member(V, 'length_squared', S if not is_float else 'float', [], f'return {total(comps, "self.{c} * self.{c}")};')
    member(V, 'length', 'float', [], f'return (ls_float)sqrt((double)({total(comps, "self.{c} * self.{c}")}));')
    member(V, 'dot', S, [(V, 'other')], f'return {total(comps, "self.{c} * other.{c}")};')
    member(V, 'distance_squared', S, [(V, 'other')], f'{L} d = ls_{V}_op_sub(self, other); return ls_{V}_length_squared(d);')
    member(V, 'distance', 'float', [(V, 'other')], f'return ls_{V}_length(ls_{V}_op_sub(self, other));')
    member(V, 'min', V, [(V, 'other')], f'return {lit("self.{c} < other.{c} ? self.{c} : other.{c}")};')
    member(V, 'max', V, [(V, 'other')], f'return {lit("self.{c} > other.{c} ? self.{c} : other.{c}")};')
    member(V, 'clamp', V, [(V, 'min'), (V, 'max')],
           f'return {lit("self.{c} < min.{c} ? min.{c} : self.{c} > max.{c} ? max.{c} : self.{c}")};')
    member(V, 'abs', V, [], f'return {lit("self.{c} < 0 ? -self.{c} : self.{c}")};')
    member(V, 'sign', V, [], f'return {lit("(" + CS + ")(self.{c} > 0 ? 1 : self.{c} < 0 ? -1 : 0)")};')
    if n == 3:
        member(V, 'xy', 'Vec2' + ('' if is_float else 'i'), [], f'return (LS_{"Vec2" + ("" if is_float else "i")}){{ self.x, self.y }};')
    member(V, 'to_float' if not is_float else 'to_int', V + 'i' if is_float else V[:-1], [],
           f'return (LS_{V + "i" if is_float else V[:-1]}){{ {cw(comps, "(" + ("int" if is_float else "float") + ")self.{c}")} }};')

    operator(V, '+', V, V, f'ls_{V}_op_add', f'return {lit("a.{c} + b.{c}")};')
    operator(V, '-', V, V, f'ls_{V}_op_sub', f'return {lit("a.{c} - b.{c}")};')
    operator(V, '*', V, V, f'ls_{V}_op_mul', f'return {lit("a.{c} * b.{c}")};')
    operator(V, '-', '', V, f'ls_{V}_op_neg', f'return {lit("-a.{c}")};')
    scalars = ['float', 'int'] if is_float else ['int']
    for s in scalars:
        operator(V, '*', s, V, f'ls_{V}_op_mul_{s}', f'return {lit(cast + "(a.{c} * b)")};', [(V, 'a'), (s, 'b')])
        operator(s, '*', V, V, f'ls_{V}_op_{s}_mul', f'return {lit(cast + "(a * b.{c})")};', [(s, 'a'), (V, 'b')])
    if is_float:
        operator(V, '/', V, V, f'ls_{V}_op_div', f'return {lit("a.{c} / b.{c}")};')
        for s in scalars:
            operator(V, '/', s, V, f'ls_{V}_op_div_{s}', f'return {lit("a.{c} / (float)b")};', [(V, 'a'), (s, 'b')])
    else:
        operator(V, '/', 'int', V, f'ls_{V}_op_div_int', f'return {lit("(int)(b ? a.{c} / b : 0)")};', [(V, 'a'), ('int', 'b')])
        operator(V, '%', 'int', V, f'ls_{V}_op_mod_int', f'return {lit("(int)(b ? a.{c} % b : 0)")};', [(V, 'a'), ('int', 'b')])
    func(f'ls_{V}_eq', 'bool', [(V, 'a'), (V, 'b')], f'return {" && ".join(f"a.{c} == b.{c}" for c in comps)};')

    if is_float:
        member(V, 'normalize', V, [], f'ls_float l = ls_{V}_length(self); return l > 0 ? ls_{V}_op_div_float(self, l) : self;')
        member(V, 'mix', V, [(V, 'other'), ('float', 't')], f'return {lit("(float)(self.{c} + (other.{c} - self.{c}) * t)")};')
        member(V, 'lerp', V, [(V, 'other'), ('float', 't')], f'return ls_{V}_mix(self, other, t);')
        member(V, 'floor', V, [], f'return {lit("(float)floor(self.{c})")};')
        member(V, 'ceil', V, [], f'return {lit("(float)ceil(self.{c})")};')
        member(V, 'round', V, [], f'return {lit("(float)ls_round(self.{c})")};')
        member(V, 'frac', V, [], f'return {lit("(float)(self.{c} - floor(self.{c}))")};')
        member(V, 'reflect', V, [(V, 'normal')], f'ls_float d = 2 * ls_{V}_dot(self, normal); return {lit("(float)(self.{c} - d * normal.{c})")};')
        member(V, 'project', V, [(V, 'onto')], f'ls_float d = ls_{V}_dot(onto, onto); return d > 0 ? ls_{V}_op_mul_float(onto, ls_{V}_dot(self, onto) / d) : ls_{V}_zero();')
        member(V, 'angle', 'float', [(V, 'other')],
               f'ls_float d = ls_{V}_length(self) * ls_{V}_length(other); return d > 0 ? ls_acos_clamped(ls_{V}_dot(self, other) / d) : 0.0;')
        member(V, 'move_towards', V, [(V, 'target'), ('float', 'max_delta')],
               f'{L} d = ls_{V}_op_sub(target, self); ls_float l = ls_{V}_length(d); return (l <= max_delta || l == 0) ? target : ls_{V}_op_add(self, ls_{V}_op_mul_float(d, max_delta / l));')
        member(V, 'approx', 'bool', [(V, 'other')], f'return {" && ".join(f"ls_abs_f(self.{c} - other.{c}) < 1e-5" for c in comps)};')
        if n == 3:
            member(V, 'cross', V, [(V, 'other')],
                   f'return ({L}){{ self.y * other.z - self.z * other.y, self.z * other.x - self.x * other.z, self.x * other.y - self.y * other.x }};')
            member(V, 'to_vec2', 'Vec2', [], 'return (LS_Vec2){ self.x, self.y };')
        else:
            member(V, 'perp', V, [], f'return ({L}){{ -self.y, self.x }};')
            member(V, 'to_vec3', 'Vec3', [('float', 'z')], 'return (LS_Vec3){ self.x, self.y, (float)z };', defaults={'z': '0'})


def vectors():
    for name, comps, is_float in (('Vec2', 'xy', True), ('Vec3', 'xyz', True), ('Vec2i', 'xy', False), ('Vec3i', 'xyz', False)):
        vector(name, comps, is_float)


# ---------------------------------------------------------------- quaternion and transform

def quat():
    Q = 'Quat'
    for c in 'wxyz': field(Q, c, 'float')
    member(Q, 'new', Q, [('float', 'w'), ('float', 'x'), ('float', 'y'), ('float', 'z')],
           'return (LS_Quat){ (float)x, (float)y, (float)z, (float)w };', static=True, defaults={'w': '1', 'x': '0', 'y': '0', 'z': '0'})
    member(Q, 'identity', Q, [], 'return (LS_Quat){ 0, 0, 0, 1 };', static=True)
    member(Q, 'from_euler', Q, [('Vec3', 'euler')],
           'double cx = cos(euler.x * 0.5), sx = sin(euler.x * 0.5), cy = cos(euler.y * 0.5), sy = sin(euler.y * 0.5), cz = cos(euler.z * 0.5), sz = sin(euler.z * 0.5);'
           ' return (LS_Quat){ (float)(sx * cy * cz - cx * sy * sz), (float)(cx * sy * cz + sx * cy * sz), (float)(cx * cy * sz - sx * sy * cz), (float)(cx * cy * cz + sx * sy * sz) };',
           static=True)
    member(Q, 'from_axis_angle', Q, [('Vec3', 'axis'), ('float', 'angle')],
           'LS_Vec3 n = ls_Vec3_normalize(axis); double s = sin(angle * 0.5); return (LS_Quat){ (float)(n.x * s), (float)(n.y * s), (float)(n.z * s), (float)cos(angle * 0.5) };',
           static=True)
    member(Q, 'from_to', Q, [('Vec3', 'from'), ('Vec3', 'to')],
           'LS_Vec3 a = ls_Vec3_normalize(from), b = ls_Vec3_normalize(to); ls_float d = ls_Vec3_dot(a, b);'
           ' if (d >= 1.0 - 1e-9) return (LS_Quat){ 0, 0, 0, 1 };'
           ' if (d <= -1.0 + 1e-9) { LS_Vec3 axis = ls_Vec3_cross(ls_Vec3_unit_x(), a); if (ls_Vec3_length_squared(axis) < 1e-6) axis = ls_Vec3_cross(ls_Vec3_unit_y(), a); return ls_Quat_from_axis_angle(axis, 3.14159265358979323846); }'
           ' { LS_Vec3 c = ls_Vec3_cross(a, b); return ls_Quat_normalize((LS_Quat){ c.x, c.y, c.z, (float)(1.0 + d) }); }',
           static=True)
    C.append('static LS_Quat ls_Quat_from_columns(LS_Vec3 c0, LS_Vec3 c1, LS_Vec3 c2) {'
             ' double m00 = c0.x, m01 = c1.x, m02 = c2.x, m10 = c0.y, m11 = c1.y, m12 = c2.y, m20 = c0.z, m21 = c1.z, m22 = c2.z; double t = m00 + m11 + m22; LS_Quat q;'
             ' if (t > 0) { double s = sqrt(t + 1.0) * 2; q.w = (float)(0.25 * s); q.x = (float)((m21 - m12) / s); q.y = (float)((m02 - m20) / s); q.z = (float)((m10 - m01) / s); }'
             ' else if (m00 > m11 && m00 > m22) { double s = sqrt(1.0 + m00 - m11 - m22) * 2; q.w = (float)((m21 - m12) / s); q.x = (float)(0.25 * s); q.y = (float)((m01 + m10) / s); q.z = (float)((m02 + m20) / s); }'
             ' else if (m11 > m22) { double s = sqrt(1.0 + m11 - m00 - m22) * 2; q.w = (float)((m02 - m20) / s); q.x = (float)((m01 + m10) / s); q.y = (float)(0.25 * s); q.z = (float)((m12 + m21) / s); }'
             ' else { double s = sqrt(1.0 + m22 - m00 - m11) * 2; q.w = (float)((m10 - m01) / s); q.x = (float)((m02 + m20) / s); q.y = (float)((m12 + m21) / s); q.z = (float)(0.25 * s); }'
             ' return ls_Quat_normalize(q); }')
    member(Q, 'look_rotation', Q, [('Vec3', 'forward'), ('Vec3', 'up')],
           'LS_Vec3 f = ls_Vec3_normalize(forward); LS_Vec3 r = ls_Vec3_normalize(ls_Vec3_cross(up, f)); LS_Vec3 u = ls_Vec3_cross(f, r); return ls_Quat_from_columns(r, u, f);',
           static=True)
    member(Q, 'length', 'float', [], 'return sqrt((double)self.x * self.x + (double)self.y * self.y + (double)self.z * self.z + (double)self.w * self.w);')
    member(Q, 'dot', 'float', [('Quat', 'other')], 'return (double)self.x * other.x + (double)self.y * other.y + (double)self.z * other.z + (double)self.w * other.w;')
    member(Q, 'normalize', Q, [], 'ls_float l = ls_Quat_length(self); return l > 0 ? (LS_Quat){ (float)(self.x / l), (float)(self.y / l), (float)(self.z / l), (float)(self.w / l) } : (LS_Quat){ 0, 0, 0, 1 };')
    member(Q, 'conjugate', Q, [], 'return (LS_Quat){ -self.x, -self.y, -self.z, self.w };')
    member(Q, 'inverse', Q, [], 'ls_float d = ls_Quat_dot(self, self); return d > 0 ? (LS_Quat){ (float)(-self.x / d), (float)(-self.y / d), (float)(-self.z / d), (float)(self.w / d) } : self;')
    member(Q, 'to_euler', 'Vec3', [],
           'double py = 2.0 * (self.y * self.z + self.w * self.x), px = (double)self.w * self.w - (double)self.x * self.x - (double)self.y * self.y + (double)self.z * self.z;'
           ' double pitch = (ls_abs_f(px) < 1e-12 && ls_abs_f(py) < 1e-12) ? 2.0 * atan2(self.x, self.w) : atan2(py, px);'
           ' double yaw = ls_asin_clamped(-2.0 * ((double)self.x * self.z - (double)self.w * self.y));'
           ' double roll = atan2(2.0 * ((double)self.x * self.y + (double)self.w * self.z), (double)self.w * self.w + (double)self.x * self.x - (double)self.y * self.y - (double)self.z * self.z);'
           ' return (LS_Vec3){ (float)pitch, (float)yaw, (float)roll };')
    member(Q, 'rotate', 'Vec3', [('Vec3', 'v')],
           'LS_Vec3 q = { self.x, self.y, self.z }; LS_Vec3 t = ls_Vec3_op_mul_float(ls_Vec3_cross(q, v), 2); return ls_Vec3_op_add(ls_Vec3_op_add(v, ls_Vec3_op_mul_float(t, self.w)), ls_Vec3_cross(q, t));')
    member(Q, 'slerp', Q, [('Quat', 'other'), ('float', 'alpha')],
           'LS_Quat z = other; double cosTheta = ls_Quat_dot(self, other);'
           ' if (cosTheta < 0) { z = (LS_Quat){ -other.x, -other.y, -other.z, -other.w }; cosTheta = -cosTheta; }'
           ' if (cosTheta > 1.0 - 1e-6) return ls_Quat_nlerp(self, z, alpha);'
           ' { double angle = acos(cosTheta), a = sin((1.0 - alpha) * angle) / sin(angle), b = sin(alpha * angle) / sin(angle);'
           ' return (LS_Quat){ (float)(a * self.x + b * z.x), (float)(a * self.y + b * z.y), (float)(a * self.z + b * z.z), (float)(a * self.w + b * z.w) }; }')
    member(Q, 'nlerp', Q, [('Quat', 'other'), ('float', 'alpha')],
           'LS_Quat z = ls_Quat_dot(self, other) < 0 ? (LS_Quat){ -other.x, -other.y, -other.z, -other.w } : other;'
           ' return ls_Quat_normalize((LS_Quat){ (float)(self.x + (z.x - self.x) * alpha), (float)(self.y + (z.y - self.y) * alpha), (float)(self.z + (z.z - self.z) * alpha), (float)(self.w + (z.w - self.w) * alpha) });')
    member(Q, 'delta_to', Q, [('Quat', 'other')], 'return ls_Quat_op_mul(other, ls_Quat_inverse(self));')
    member(Q, 'angle', 'float', [('Quat', 'other')], 'ls_float d = ls_abs_f(ls_Quat_dot(ls_Quat_normalize(self), ls_Quat_normalize(other))); return 2.0 * ls_acos_clamped(d);')
    operator(Q, '*', Q, Q, 'ls_Quat_op_mul',
             'return (LS_Quat){ (float)((double)a.w * b.x + (double)a.x * b.w + (double)a.y * b.z - (double)a.z * b.y),'
             ' (float)((double)a.w * b.y + (double)a.y * b.w + (double)a.z * b.x - (double)a.x * b.z),'
             ' (float)((double)a.w * b.z + (double)a.z * b.w + (double)a.x * b.y - (double)a.y * b.x),'
             ' (float)((double)a.w * b.w - (double)a.x * b.x - (double)a.y * b.y - (double)a.z * b.z) };')
    operator(Q, '*', 'Vec3', 'Vec3', 'ls_Quat_op_mul_vec3', 'return ls_Quat_rotate(a, b);')
    func('ls_Quat_eq', 'bool', [(Q, 'a'), (Q, 'b')], 'return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;')


def mat4():
    T = 'Mat4x4'
    # Column-major like glm: element (row r, column c) is m[c * 4 + r].
    C.append('static double ls_Mat4x4_minor3(const float* m, int row, int col) {'
             ' double a[9]; int i = 0, r, c;'
             ' for (c = 0; c < 4; ++c) { if (c == col) continue; for (r = 0; r < 4; ++r) { if (r == row) continue; a[i++] = m[c * 4 + r]; } }'
             ' return a[0] * (a[4] * a[8] - a[7] * a[5]) - a[3] * (a[1] * a[8] - a[7] * a[2]) + a[6] * (a[1] * a[5] - a[4] * a[2]); }')
    C.append('static double ls_Mat4x4_cofactor(const float* m, int row, int col) { double minor = ls_Mat4x4_minor3(m, row, col); return ((row + col) & 1) ? -minor : minor; }')
    member(T, 'new', T, [], 'return ls_Mat4x4_identity();', static=True)
    member(T, 'identity', T, [], 'LS_Mat4x4 r = {{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }}; return r;', static=True)
    member(T, 'from_pos', T, [('Vec3', 'position')], 'LS_Mat4x4 r = ls_Mat4x4_identity(); r.m[12] = position.x; r.m[13] = position.y; r.m[14] = position.z; return r;', static=True)
    member(T, 'from_pos_rot_scale', T, [('Vec3', 'position'), ('Quat', 'rotation'), ('Vec3', 'scale')],
           'double x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w; double xx = x * x, yy = y * y, zz = z * z, xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;'
           ' LS_Mat4x4 r = ls_Mat4x4_identity();'
           ' r.m[0] = (float)((1 - 2 * (yy + zz)) * scale.x); r.m[1] = (float)(2 * (xy + wz) * scale.x); r.m[2] = (float)(2 * (xz - wy) * scale.x);'
           ' r.m[4] = (float)(2 * (xy - wz) * scale.y); r.m[5] = (float)((1 - 2 * (xx + zz)) * scale.y); r.m[6] = (float)(2 * (yz + wx) * scale.y);'
           ' r.m[8] = (float)(2 * (xz + wy) * scale.z); r.m[9] = (float)(2 * (yz - wx) * scale.z); r.m[10] = (float)((1 - 2 * (xx + yy)) * scale.z);'
           ' r.m[12] = position.x; r.m[13] = position.y; r.m[14] = position.z; return r;', static=True)
    member(T, 'from_pos_rot', T, [('Vec3', 'position'), ('Quat', 'rotation')], 'return ls_Mat4x4_from_pos_rot_scale(position, rotation, (LS_Vec3){ 1, 1, 1 });', static=True)
    member(T, 'look_at', T, [('Vec3', 'eye'), ('Vec3', 'target'), ('Vec3', 'up')],
           'return ls_Mat4x4_from_pos_rot(eye, ls_Quat_look_rotation(ls_Vec3_op_sub(target, eye), up));', static=True)
    member(T, 'translation', 'Vec3', [], 'return (LS_Vec3){ self.m[12], self.m[13], self.m[14] };')
    member(T, 'scale', 'Vec3', [],
           'return (LS_Vec3){ (float)sqrt((double)self.m[0] * self.m[0] + (double)self.m[1] * self.m[1] + (double)self.m[2] * self.m[2]),'
           ' (float)sqrt((double)self.m[4] * self.m[4] + (double)self.m[5] * self.m[5] + (double)self.m[6] * self.m[6]),'
           ' (float)sqrt((double)self.m[8] * self.m[8] + (double)self.m[9] * self.m[9] + (double)self.m[10] * self.m[10]) };')
    member(T, 'rotation', 'Quat', [],
           'LS_Vec3 s = ls_Mat4x4_scale(self);'
           ' LS_Vec3 c0 = { s.x > 0 ? self.m[0] / s.x : 1, s.x > 0 ? self.m[1] / s.x : 0, s.x > 0 ? self.m[2] / s.x : 0 };'
           ' LS_Vec3 c1 = { s.y > 0 ? self.m[4] / s.y : 0, s.y > 0 ? self.m[5] / s.y : 1, s.y > 0 ? self.m[6] / s.y : 0 };'
           ' LS_Vec3 c2 = { s.z > 0 ? self.m[8] / s.z : 0, s.z > 0 ? self.m[9] / s.z : 0, s.z > 0 ? self.m[10] / s.z : 1 };'
           ' return ls_Quat_from_columns(c0, c1, c2);')
    member(T, 'set_translation', T, [('Vec3', 'position')], 'self.m[12] = position.x; self.m[13] = position.y; self.m[14] = position.z; return self;')
    member(T, 'set_rotation', T, [('Quat', 'rotation')], 'return ls_Mat4x4_from_pos_rot_scale(ls_Mat4x4_translation(self), rotation, ls_Mat4x4_scale(self));')
    member(T, 'set_scale', T, [('Vec3', 'scale')], 'return ls_Mat4x4_from_pos_rot_scale(ls_Mat4x4_translation(self), ls_Mat4x4_rotation(self), scale);')
    member(T, 'transform_point', 'Vec3', [('Vec3', 'point')],
           'return (LS_Vec3){ self.m[0] * point.x + self.m[4] * point.y + self.m[8] * point.z + self.m[12],'
           ' self.m[1] * point.x + self.m[5] * point.y + self.m[9] * point.z + self.m[13],'
           ' self.m[2] * point.x + self.m[6] * point.y + self.m[10] * point.z + self.m[14] };')
    member(T, 'transform_direction', 'Vec3', [('Vec3', 'direction')],
           'return (LS_Vec3){ self.m[0] * direction.x + self.m[4] * direction.y + self.m[8] * direction.z,'
           ' self.m[1] * direction.x + self.m[5] * direction.y + self.m[9] * direction.z,'
           ' self.m[2] * direction.x + self.m[6] * direction.y + self.m[10] * direction.z };')
    member(T, 'determinant', 'float', [],
           'return self.m[0] * ls_Mat4x4_cofactor(self.m, 0, 0) + self.m[4] * ls_Mat4x4_cofactor(self.m, 0, 1) + self.m[8] * ls_Mat4x4_cofactor(self.m, 0, 2) + self.m[12] * ls_Mat4x4_cofactor(self.m, 0, 3);')
    member(T, 'transpose', T, [], 'LS_Mat4x4 r; int c, k; for (c = 0; c < 4; ++c) for (k = 0; k < 4; ++k) r.m[c * 4 + k] = self.m[k * 4 + c]; return r;')
    member(T, 'inverse', T, [],
           'ls_float det = ls_Mat4x4_determinant(self); LS_Mat4x4 r; int c, k;'
           ' if (det == 0) return ls_Mat4x4_identity();'
           ' for (c = 0; c < 4; ++c) for (k = 0; k < 4; ++k) r.m[c * 4 + k] = (float)(ls_Mat4x4_cofactor(self.m, c, k) / det); return r;')
    member(T, 'inverse_transform_point', 'Vec3', [('Vec3', 'point')], 'return ls_Mat4x4_transform_point(ls_Mat4x4_inverse(self), point);')
    member(T, 'mul', T, [('Mat4x4', 'other')],
           'LS_Mat4x4 r; int c, k, j; for (c = 0; c < 4; ++c) for (k = 0; k < 4; ++k) { double sum = 0; for (j = 0; j < 4; ++j) sum += (double)self.m[j * 4 + k] * other.m[c * 4 + j]; r.m[c * 4 + k] = (float)sum; } return r;')
    member(T, 'decompose', 'table', [],
           'ls_table t = ls_table_create(); LS_Vec3 p = ls_Mat4x4_translation(self), s = ls_Mat4x4_scale(self); LS_Quat q = ls_Mat4x4_rotation(self);'
           ' ls_table_set(t, ls_box_pointer(LS_KIND_STRING, ls_intern("translation")), ls_box_struct(&ls_type_Vec3, &p));'
           ' ls_table_set(t, ls_box_pointer(LS_KIND_STRING, ls_intern("rotation")), ls_box_struct(&ls_type_Quat, &q));'
           ' ls_table_set(t, ls_box_pointer(LS_KIND_STRING, ls_intern("scale")), ls_box_struct(&ls_type_Vec3, &s)); return t;')
    operator(T, '*', T, T, 'ls_Mat4x4_op_mul', 'return ls_Mat4x4_mul(a, b);')
    operator(T, '*', 'Vec3', 'Vec3', 'ls_Mat4x4_op_mul_vec3', 'return ls_Mat4x4_transform_point(a, b);')
    func('ls_Mat4x4_eq', 'bool', [(T, 'a'), (T, 'b')], 'int i; for (i = 0; i < 16; ++i) if (a.m[i] != b.m[i]) return 0; return 1;')


def defaults():
    for v in ('Vec2', 'Vec3', 'Vec2i', 'Vec3i'):
        func(f'ls_{v}_default', v, [], f'return ls_{v}_zero();')
    func('ls_Quat_default', 'Quat', [], 'return ls_Quat_identity();')
    func('ls_Mat4x4_default', 'Mat4x4', [], 'return ls_Mat4x4_identity();')


# ---------------------------------------------------------------- math class

def math_class():
    M = 'math'

    def fn(name, ret, params, body, **kw):
        member(M, name, ret, params, body, static=True, cname=f'ls_math_{name}', **kw)

    for name, expr in (('pi', '3.14159265358979323846'), ('tau', '6.28318530717958647692'), ('epsilon', '2.220446049250313e-16'),
                       ('deg2rad', '0.01745329251994329577'), ('rad2deg', '57.29577951308232087680')):
        constant(M, name, 'float', f'ls_math_{name}()', f'return {expr};')
    constant(M, 'huge', 'float', 'ls_math_huge()', 'volatile double big = 1.7976931348623157e308; return big * 2.0;')
    F = 'float'
    fn('abs', 'int', [('int', 'x')], 'return x < 0 ? ls_int_neg(x) : x;')
    fn('abs', F, [(F, 'x')], 'return x < 0 ? -x : x;')
    fn('sign', 'int', [('int', 'x')], 'return x > 0 ? 1 : x < 0 ? -1 : 0;')
    fn('sign', F, [(F, 'x')], 'return x > 0 ? 1.0 : x < 0 ? -1.0 : 0.0;')
    fn('floor', F, [(F, 'x')], 'return floor(x);')
    fn('ceil', F, [(F, 'x')], 'return ceil(x);')
    fn('round', F, [(F, 'x')], 'return ls_round(x);')
    fn('trunc', F, [(F, 'x')], 'return x < 0 ? ceil(x) : floor(x);')
    fn('frac', F, [(F, 'x')], 'return x - floor(x);')
    fn('floor_to_int', 'int', [(F, 'x')], 'return (ls_int)floor(x);')
    fn('ceil_to_int', 'int', [(F, 'x')], 'return (ls_int)ceil(x);')
    fn('round_to_int', 'int', [(F, 'x')], 'return (ls_int)ls_round(x);')
    fn('sqrt', F, [(F, 'x')], 'return sqrt(x);')
    fn('pow', F, [(F, 'x'), (F, 'y')], 'return pow(x, y);')
    fn('exp', F, [(F, 'x')], 'return exp(x);')
    fn('log', F, [(F, 'x')], 'return log(x);')
    fn('log', F, [(F, 'x'), (F, 'base')], 'return log(x) / log(base);')
    fn('log2', F, [(F, 'x')], 'return log(x) / 0.69314718055994530942;')
    fn('log10', F, [(F, 'x')], 'return log10(x);')
    for name in ('sin', 'cos', 'tan', 'asin', 'acos', 'atan'):
        fn(name, F, [(F, 'x')], f'return {name}(x);')
    fn('atan2', F, [(F, 'y'), (F, 'x')], 'return atan2(y, x);')
    fn('radians', F, [(F, 'degrees')], 'return degrees * 0.01745329251994329577;')
    fn('degrees', F, [(F, 'radians')], 'return radians * 57.29577951308232087680;')
    fn('mod', F, [(F, 'x'), (F, 'y')], 'return y != 0 ? x - y * floor(x / y) : 0.0;')
    fn('min', 'int', [('int', 'a'), ('int', 'b')], 'return a < b ? a : b;')
    fn('min', F, [(F, 'a'), (F, 'b')], 'return a < b ? a : b;')
    fn('max', 'int', [('int', 'a'), ('int', 'b')], 'return a > b ? a : b;')
    fn('max', F, [(F, 'a'), (F, 'b')], 'return a > b ? a : b;')
    fn('clamp', 'int', [('int', 'x'), ('int', 'lo'), ('int', 'hi')], 'return x < lo ? lo : x > hi ? hi : x;')
    fn('clamp', F, [(F, 'x'), (F, 'lo'), (F, 'hi')], 'return x < lo ? lo : x > hi ? hi : x;')
    fn('saturate', F, [(F, 'x')], 'return x < 0 ? 0.0 : x > 1 ? 1.0 : x;')
    fn('lerp', F, [(F, 'a'), (F, 'b'), (F, 't')], 'return a + (b - a) * t;')
    fn('inverse_lerp', F, [(F, 'a'), (F, 'b'), (F, 'value')], 'return a != b ? (value - a) / (b - a) : 0.0;')
    fn('remap', F, [(F, 'value'), (F, 'from_lo'), (F, 'from_hi'), (F, 'to_lo'), (F, 'to_hi')],
       'ls_float t = from_lo != from_hi ? (value - from_lo) / (from_hi - from_lo) : 0.0; return to_lo + (to_hi - to_lo) * t;')
    fn('smoothstep', F, [(F, 'edge0'), (F, 'edge1'), (F, 'x')],
       'ls_float t = edge0 != edge1 ? (x - edge0) / (edge1 - edge0) : 0.0; t = t < 0 ? 0.0 : t > 1 ? 1.0 : t; return t * t * (3.0 - 2.0 * t);')
    fn('move_towards', F, [(F, 'current'), (F, 'target'), (F, 'max_delta')],
       'ls_float d = target - current; return ls_abs_f(d) <= max_delta ? target : current + (d > 0 ? max_delta : -max_delta);')
    fn('approx', 'bool', [(F, 'a'), (F, 'b')], 'return ls_abs_f(a - b) < 1e-9;')
    fn('approx', 'bool', [(F, 'a'), (F, 'b'), (F, 'epsilon')], 'return ls_abs_f(a - b) <= epsilon;')
    fn('is_nan', 'bool', [(F, 'x')], 'return x != x;')

    for V in ('Vec2', 'Vec3'):
        for name in ('abs', 'sign', 'floor', 'ceil', 'round', 'frac', 'normalize'):
            fn(name, V, [(V, 'v')], f'return ls_{V}_{name}(v);')
        fn('min', V, [(V, 'a'), (V, 'b')], f'return ls_{V}_min(a, b);')
        fn('max', V, [(V, 'a'), (V, 'b')], f'return ls_{V}_max(a, b);')
        fn('clamp', V, [(V, 'v'), (V, 'lo'), (V, 'hi')], f'return ls_{V}_clamp(v, lo, hi);')
        fn('lerp', V, [(V, 'a'), (V, 'b'), (F, 't')], f'return ls_{V}_mix(a, b, t);')
        fn('dot', F, [(V, 'a'), (V, 'b')], f'return ls_{V}_dot(a, b);')
        fn('length', F, [(V, 'v')], f'return ls_{V}_length(v);')
        fn('length_squared', F, [(V, 'v')], f'return ls_{V}_length_squared(v);')
        fn('distance', F, [(V, 'a'), (V, 'b')], f'return ls_{V}_distance(a, b);')
        fn('distance_squared', F, [(V, 'a'), (V, 'b')], f'return ls_{V}_distance_squared(a, b);')
        fn('reflect', V, [(V, 'v'), (V, 'normal')], f'return ls_{V}_reflect(v, normal);')
        fn('project', V, [(V, 'v'), (V, 'onto')], f'return ls_{V}_project(v, onto);')
        fn('angle', F, [(V, 'a'), (V, 'b')], f'return ls_{V}_angle(a, b);')
        fn('move_towards', V, [(V, 'current'), (V, 'target'), (F, 'max_delta')], f'return ls_{V}_move_towards(current, target, max_delta);')
    fn('cross', 'Vec3', [('Vec3', 'a'), ('Vec3', 'b')], 'return ls_Vec3_cross(a, b);')
    for V in ('Vec2i', 'Vec3i'):
        for name in ('abs', 'sign'):
            fn(name, V, [(V, 'v')], f'return ls_{V}_{name}(v);')
        fn('min', V, [(V, 'a'), (V, 'b')], f'return ls_{V}_min(a, b);')
        fn('max', V, [(V, 'a'), (V, 'b')], f'return ls_{V}_max(a, b);')
        fn('clamp', V, [(V, 'v'), (V, 'lo'), (V, 'hi')], f'return ls_{V}_clamp(v, lo, hi);')
        fn('dot', 'int', [(V, 'a'), (V, 'b')], f'return ls_{V}_dot(a, b);')
        fn('length', F, [(V, 'v')], f'return ls_{V}_length(v);')
        fn('length_squared', 'int', [(V, 'v')], f'return ls_{V}_length_squared(v);')
        fn('distance', F, [(V, 'a'), (V, 'b')], f'return ls_{V}_distance(a, b);')
        fn('distance_squared', 'int', [(V, 'a'), (V, 'b')], f'return ls_{V}_distance_squared(a, b);')
    fn('dot', F, [('Quat', 'a'), ('Quat', 'b')], 'return ls_Quat_dot(a, b);')
    fn('slerp', 'Quat', [('Quat', 'a'), ('Quat', 'b'), (F, 't')], 'return ls_Quat_slerp(a, b, t);')
    fn('nlerp', 'Quat', [('Quat', 'a'), ('Quat', 'b'), (F, 't')], 'return ls_Quat_nlerp(a, b, t);')
    fn('angle', F, [('Quat', 'a'), ('Quat', 'b')], 'return ls_Quat_angle(a, b);')
    fn('inverse', 'Quat', [('Quat', 'q')], 'return ls_Quat_inverse(q);')
    fn('normalize', 'Quat', [('Quat', 'q')], 'return ls_Quat_normalize(q);')


# ---------------------------------------------------------------- emission

HEADER = '''/* GENERATED by LuaSharp/scripts/gen_vecmath.py. Do not edit by hand. */
#ifndef LUASHARP_VECMATH_H
#define LUASHARP_VECMATH_H

/* Native value types. Fields are float32 / int32 and the layouts are stable (see docs/native-abi.md). */
typedef struct LS_Vec2 { float x, y; } LS_Vec2;
typedef struct LS_Vec3 { float x, y, z; } LS_Vec3;
typedef struct LS_Vec2i { int x, y; } LS_Vec2i;
typedef struct LS_Vec3i { int x, y, z; } LS_Vec3i;
typedef struct LS_Quat { float x, y, z, w; } LS_Quat;
typedef struct LS_Mat4x4 { float m[16]; } LS_Mat4x4; /* column-major, like glm: element (row r, column c) is m[c * 4 + r] */

double sqrt(double); double sin(double); double cos(double); double tan(double);
double asin(double); double acos(double); double atan(double); double atan2(double, double);
double floor(double); double ceil(double); double pow(double, double); double exp(double);
double log(double); double log10(double);

static ls_float ls_abs_f(ls_float x) { return x < 0 ? -x : x; }
static ls_float ls_round(ls_float x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }
static ls_float ls_acos_clamped(ls_float x) { return acos(x < -1 ? -1 : x > 1 ? 1 : x); }
static ls_float ls_asin_clamped(ls_float x) { return asin(x < -1 ? -1 : x > 1 ? 1 : x); }
'''

STRUCT_INFO = [
    ('Vec2', 'LS_Vec2', [('x', 'LS_SLOT_FLOAT32'), ('y', 'LS_SLOT_FLOAT32')]),
    ('Vec3', 'LS_Vec3', [('x', 'LS_SLOT_FLOAT32'), ('y', 'LS_SLOT_FLOAT32'), ('z', 'LS_SLOT_FLOAT32')]),
    ('Vec2i', 'LS_Vec2i', [('x', 'LS_SLOT_INT32'), ('y', 'LS_SLOT_INT32')]),
    ('Vec3i', 'LS_Vec3i', [('x', 'LS_SLOT_INT32'), ('y', 'LS_SLOT_INT32'), ('z', 'LS_SLOT_INT32')]),
    ('Quat', 'LS_Quat', [('w', 'LS_SLOT_FLOAT32'), ('x', 'LS_SLOT_FLOAT32'), ('y', 'LS_SLOT_FLOAT32'), ('z', 'LS_SLOT_FLOAT32')]),
    ('Mat4x4', 'LS_Mat4x4', [(f'm{i}', 'LS_SLOT_FLOAT32') for i in range(16)]),
]


def type_infos():
    lines = []
    for name, ctype, fields in STRUCT_INFO:
        lines.append(f'static const LS_TypeInfo ls_type_{name};')
    for name, ctype, fields in STRUCT_INFO:
        rows = []
        for f in fields:
            nested = f'&ls_type_{f[2]}' if len(f) > 2 else '(const LS_TypeInfo*)0'
            off = f'm[{f[0][1:]}]' if name == 'Mat4x4' else f[0]
            rows.append(f'    {{"{f[0]}", LS_STRUCT_PAYLOAD + ls_offsetof({ctype}, {off}), {f[1]}, 0, {nested}}},')
        lines.append(f'static const LS_FieldInfo ls_fields_{name}[] = {{\n' + '\n'.join(rows) + '\n};')
        lines.append(f'static const LS_TypeInfo ls_type_{name} = {{"{name}", {len(fields)}, ls_fields_{name}, 0, (const LS_MethodInfo*)0, sizeof({ctype})}};')
    return '\n'.join(lines)


def document():
    lines = ['# LuaSharp math types and `math` library', '',
             'Generated by `scripts/gen_vecmath.py`. Do not edit by hand.', '',
             'Instance members are written `receiver.name(...)`. Static members are written `Type.name(...)`. A parameter followed by `=` has a default value for an omitted argument.', '']
    owners = []
    for entry in DOC:
        if entry[0] not in owners: owners.append(entry[0])
    for owner in owners:
        lines += [f'## {owner}', '']
        rows = [e for e in DOC if e[0] == owner]
        def sig(e):
            _, name, ret, params, kind, static, defaults = e
            if kind == 'Field': return f'`{owner}.{name}`: `{ret}`' if static else f'`{name}`: `{ret}`'
            args = ', '.join(f'{t} {n}' + (f' = {defaults[n]}' if n in defaults else '') for t, n in params)
            return f'`{owner}.{name}({args})` returns `{ret}`' if static else f'`{name}({args})` returns `{ret}`'
        fields = [e for e in rows if e[4] == 'Field' and not e[5]]
        constants = [e for e in rows if e[4] == 'Field' and e[5]]
        statics = [e for e in rows if e[4] == 'Method' and e[5]]
        methods = [e for e in rows if e[4] == 'Method' and not e[5]]
        for title, group in (('Fields', fields), ('Constants', constants), ('Static functions', statics), ('Methods', methods)):
            if group: lines += [f'**{title}**', ''] + [f'- {sig(e)}' for e in group] + ['']
        ops = [o for o in DOC_OPS if o[0] == owner or (o[2] == owner and o[0] != owner)]
        if ops: lines += ['**Operators**', ''] + [f'- `{o[0]} {o[1]} {o[2]}` gives `{o[3]}`' if o[2] else f'- `{o[1]}{o[0]}` gives `{o[3]}`' for o in ops] + ['']
    (ROOT / 'docs' / 'math-library.md').write_text(chr(10).join(lines), newline=chr(10))


def main():
    vectors(); quat(); mat4(); math_class(); defaults()
    # Function order: helpers that others call must come first; emit in dependency-friendly order by forward-declaring all.
    protos = []
    for line in C:
        head = line[:line.index(' {')]
        protos.append(head + ';')
    text = HEADER + '\n' + '\n'.join(protos) + '\n\n' + type_infos() + '\n\n' + '\n'.join(C) + '\n\n#endif\n'
    (ROOT / 'runtime' / 'luasharp' / 'vecmath.h').write_text(text, newline='\n')
    inc = '// GENERATED by scripts/gen_vecmath.py. Do not edit by hand.\n// Members\n' + '\n'.join(ROWS) + '\n'
    (ROOT / 'src' / 'BuiltinMathMembers.inc').write_text(inc, newline='\n')
    ops = '// GENERATED by scripts/gen_vecmath.py. Do not edit by hand.\n' + '\n'.join(OPS) + '\n'
    (ROOT / 'src' / 'BuiltinMathOperators.inc').write_text(ops, newline='\n')
    document()
    print(f'{len(C)} C functions, {len(ROWS)} members, {len(OPS)} operators')


if __name__ == '__main__':
    main()
