# Model Importing

The engine uses its own model format, the importer accepts only .glb files. To import a model: open the console and type `IMPORT_MODEL <glb_path>`. `<glb_path>` is relative to the set game_folder (default to "gamedat/"). This will import the model and create a `.cmdl` and `.mis` files.

To edit the model settings, open the model editor tool. This gives various settings to override materials, keep pruned skeleton bones, include other models for animations and more. 

## Model Physics

Inside your content creation tool, prefix meshes with "CVX_" to turn them into convex collison meshes attached to the model asset. In the map editor, these can be accessed by adding a `MeshColliderComponent` to an object with a `MeshComponent`

## Model Animations

...

