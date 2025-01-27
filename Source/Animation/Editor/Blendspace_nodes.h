#pragma once
#ifdef EDITOR_BUILD
#include "Base_node.h"
#include "Basic_nodes.h"



CLASS_H_EXPLICIT_SUPER(Blendspace2d_EdNode, BaseNodeUtil_EdNode<BlendSpace2d_CFG>, Base_EdNode)

	MAKE_STANDARD_FUNCTIONS(
		"Blendspace 2D",
		BLEND_COLOR,
		"Allows placing clips in a triangulated format that blend together.\n",
	);

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK(GridData, gridpoints);
		START_PROPS(Blendspace2d_EdNode)
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef"),
			// editable version w/ custom type
			REG_STRUCT_CUSTOM_TYPE(GridPoints, PROP_EDITABLE, "BlendspaceGridEd"),
			// serialized vector version
			REG_STDVECTOR(gridpoints, PROP_SERIALIZE),
			REG_INT(cols, PROP_SERIALIZE, ""),
			REG_INT(rows, PROP_SERIALIZE, "")

		END_PROPS(Blendspace2d_EdNode)
	}

	bool compile_my_data(const AgSerializeContext* ctx) override {

		bool res = util_compile_default(this, ctx);

		// creating triangles: scan every 2x2 block and create triangles when possible
		//	then scan any unused blocks and check if its surrounded by 3 taken blocks, create a triangle then

		node->indicies.clear();
		node->verticies.clear();
		std::vector<int> gridpoint_to_vertex_index(rows*cols, -1);
		for (int y = 0; y < rows; y++) {
			for (int x = 0; x < cols; x++) {
				if (!get_by_coords(x, y).is_being_used()) continue;
				BlendSpace2d_CFG::GridPoint gp;
				gp.animation_name = get_by_coords(x, y).animation_name;
				gp.x = x / float(cols - 1);
				gp.y = x / float(rows - 1);
				node->verticies.push_back(gp);
				gridpoint_to_vertex_index[y * cols + x] = node->verticies.size() - 1;
			}
		}

		// step 1.
		const int center_x = cols / 2;
		const int center_y = rows / 2;

		for (int y = 0; y < cols - 1; y++) {
			for (int x = 0; x < rows - 1; x++) {
				bool corners[4];
				// 0 1
				// 2 3
				corners[0] = get_by_coords(x, y).is_being_used();
				corners[1] = get_by_coords(x+1, y).is_being_used();
				corners[2] = get_by_coords(x, y+1).is_being_used();
				corners[3] = get_by_coords(x+1, y + 1).is_being_used();
				int count = corners[0] + corners[1] + corners[2] + corners[3];

				// cases: if all 4 are used then triangulate so seam is tangent to center
				// if only 3 are used, create triangle
				// else skip

#define ADDTRI_VERTS(x0,y0_,x1,y1_,x2,y2) \
	node->indicies.push_back(gridpoint_to_vertex_index[y0_*cols+x0]);\
	node->indicies.push_back(gridpoint_to_vertex_index[y1_*cols+x1]);\
	node->indicies.push_back(gridpoint_to_vertex_index[y2*cols+x2]);
				if (count==4) {
					float center_this_x = x + 0.5;
					float center_this_y = y + 0.5;

					bool oposite_diagonal = true;

					if (center_this_x < center_x && center_this_y < center_y)
						oposite_diagonal = false;
					else if (center_this_x > center_x && center_this_y > center_y)
						oposite_diagonal = false;

					if (!oposite_diagonal) {
						// 2 0 1
						ADDTRI_VERTS(x, y + 1, x, y, x + 1, y);
						// 2 1 3
						ADDTRI_VERTS(x, y + 1, x + 1, y, x + 1, y+1);
					}
					else {
						// 0 1 3
						ADDTRI_VERTS(x,y,x+1,y, x+1,y+1);
						// 0 3 2
						ADDTRI_VERTS(x, y,x + 1, y + 1, x, y+1);
					}
				}
				else if (count == 3) {
#define ADDSINGLE_VERT(x_,y_)node->indicies.push_back(gridpoint_to_vertex_index[y_ * cols + x_]);
					if (corners[0])ADDSINGLE_VERT(x, y);
					if (corners[1])ADDSINGLE_VERT(x+1, y);
					if (corners[2])ADDSINGLE_VERT(x, y+1);
					if (corners[3])ADDSINGLE_VERT(x+1, y+1);
				}

			}
		}
		// step 2.

		return res;

#undef ADDSINGLE_VERT
#undef ADDTRI_VERTS
	}

	// throwaway variable as a hack for property ed system, dont use this
	int GridPoints;

	struct GridData {
		std::string animation_name;
		static const PropertyInfoList* get_props() {
			START_PROPS(GridData)
				REG_STDSTRING(animation_name, PROP_DEFAULT)
			END_PROPS(GridData)
		}

		bool is_being_used() const {
			return !animation_name.empty();
		}
	};
	int cols = 0;
	int rows = 0;
	std::vector<GridData> gridpoints;
	// 1 2 3
	// 4 5 6
	// 7 8 9
	// ...
	GridData& get_by_coords(int x, int y) {
		return gridpoints[y * cols + x];
	}
	void resize_grid(int x, int y) {
		gridpoints.clear();
		gridpoints.resize(y * x);
		cols = x;
		rows = y;
	}
	void append_row_to_grid(int row_location) {
		assert(row_location <= rows);
		const int index = row_location * cols;
		for (int i = 0; i < cols; i++) {
			gridpoints.insert(gridpoints.begin() + index, GridData());
		}
		rows++;
	}
	void append_col_to_grid(int col_location) {
		assert(col_location <= cols);
		int index = col_location;
		for (int i = 0; i < rows; i++) {
			gridpoints.insert(gridpoints.begin() + index, GridData());
			index += cols + 1;
		}
		cols++;
	}
	void remove_row_from_grid(int row_location) {
		assert(row_location <= rows);
		const int index = row_location * cols;
		for (int i = 0; i < cols; i++) {
			gridpoints.erase(gridpoints.begin() + index);
		}
		rows--;
	}
	void remove_col_from_grid(int col_location) {
		assert(col_location <= cols);
		int index = col_location;
		for (int i = 0; i < rows; i++) {
			gridpoints.erase(gridpoints.begin() + index);
			index += cols - 1;
		}
		cols--;
	}
};
#endif