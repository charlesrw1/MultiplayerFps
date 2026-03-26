Make a python script that checks _header_ include dependencies
Essentially its used to make sure modules arent becoming _coupled_ to each other
Basically a module is defined by the high level "folder" that its in so in this project, that would be Render/, LevelEditor/, Framework/ etc.
Then, I hardcode in the script the _allowed_ dependencies. For example:

Dependencies = {
	"Framework":[],
	"Render":["Framework"],
	"Game":["Render"]
	"LevelEditor":["Game"],
}

Dependencies are transative, so LevelEditor is allowed to depend on Framework.

Then iterate through all files under each module listed and count how many times headers from non-allowed dependencies are called in.
Write out in a pretty format showing modules and [GOOD] or [BAD] (...) with a count and what file included what.
	
Note thaat when you look at the #includes, you have to look at recursive includes.

You can manually parse this or use a python libtary to do this if one makes it easier.