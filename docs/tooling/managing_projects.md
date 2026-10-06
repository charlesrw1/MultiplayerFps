# Managing Projects

The engine loads a per-project `.ini` on top of `EngineVars.ini`.

1. Set `startup_project` in `EngineVars.ini`, or pass `--project <Name|path>` on the CLI.
2. A bare name such as `DevEngine` resolves to `Projects/DevEngine.ini`.
3. Project vars win over the base config. Typical project vars are `g_project_base` and `g_application_class`.

`--tests` runs never load a project ini. They set those vars in the `[game_test]` or `[editor_test]` section instead.

See [[engine_overview#Configuration (Config_Var / Engine_Cmd)]].
