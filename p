runtime {
rootdata {
root   "0"
data_used   "0"
graph_is_valid   "0"
}
nodes [
{
type "Blend_Node_CFG"
input   [
 1   -1  ]
param   "-1"
damp_factor   "0.100000"
store_value_on_reset   "0"
}
{
type "Clip_Node_CFG"
input   [
]
rm[0]   "rm::keep"
rm[1]   "rm::keep"
rm[2]   "rm::keep"
loop   "1"
speed   "1.000000"
start_frame   "0"
allow_sync   "0"
can_be_leader   "1"
clip_name   "Armature|mixamo.com|Layer0"
}
{
type "Clip_Node_CFG"
input   [
]
rm[0]   "rm::keep"
rm[1]   "rm::keep"
rm[2]   "rm::keep"
loop   "1"
speed   "1.000000"
start_frame   "0"
allow_sync   "0"
can_be_leader   "1"
clip_name   "Armature|mixamo.com|Layer0"
}
]
params [
]
}
 editor
{
rootstate {
current_id   "4"
current_layer   "1"
default_editor   "[editor]
panning=300,325

[node.0]
origin=0,0

[node.1]
origin=-69,-20

[node.2]
origin=-209,-84

[node.3]
origin=-222,-31
"
opt.open_graph   "1"
opt.open_control_params   "1"
opt.open_prop_editor   "1"
opt.statemachine_passthrough   "0"
}
nodes [
{
type "Root_EdNode"
id   "0"
graph_layer   "0"
}
{
type "Blend_EdNode"
id   "1"
graph_layer   "0"
node   "0"
}
{
type "Clip_EdNode"
id   "2"
graph_layer   "0"
node   "1"
}
{
type "Clip_EdNode"
id   "3"
graph_layer   "0"
node   "2"
}
]
}