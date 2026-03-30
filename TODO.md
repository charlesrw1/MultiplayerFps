task: implement an "asset aware" file management CLI tool
make with python. script will be itself a running repl.
commands:
	ls
	cd
	mv
	cp
	trash
	cat
	!<powershell>
	pwd
	references
	
	
The reason for this is my on disk structure stores lots of duplicate files than for human consumption
For example, textures are stored as a .tis, a source .png/.jpeg and a compiled .dds
same with models (.mis,.cmdl,.glb)

Goal: only print each asset once

ALSO: commands like cp should only copy the .mis or the .tis
trash: should remove the .tis/.png and .dds
mv: heres the big one
	need to fixup references
	so if you move a .dds, then ripgrep for all references in the asset folder and fix them
	
	
references:
	print every asset that references this one

Write a python script to do this.

Asset root: Data/ directory (or user configured)

Assset types:
texture: 
	.tis import settings
	.dds/.hdr compiled
	.png/.jpeg/.hdr source
model:
	.mis import settings
	.cmdl compiled
	.glb source
map:
	.tmap
material:
	.mm master (keep)
	.mi instance (keep)	
	.glsl output shader (hide)
	
REPL stores no state. no caching, every action is done fresh.

Note: mv should move ALL related asset files together (import settings, source, compiled).
Then fix all references to those files.
	

