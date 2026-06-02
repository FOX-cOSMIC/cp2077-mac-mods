# CLAUDE.md

This project follows the **OpenClaw multi-agent convention**. The primary entry point is `AGENTS.md` — read it first.

## Claude-Specific Notes

- **Working dir:** `$HOME/Programming/cp2077-mac-mods/`
- **Memory:** A pointer to this project is saved in your Claude memory dir (`~/.claude/projects/<home-slug>/memory/project_cp2077_mac_modding.md`)
- **Subagents:** Use the Explore subagent for codebase exploration, general-purpose for complex multi-step research, Plan for design questions
- **Skills:** `init`, `review`, `security-review`, `simplify` are available — use them when matching
- **Do not** write to `state/status.yaml` directly. Update via the `doc-keeper` agent role only.

## Quick Bring-Up

1. Read `AGENTS.md` (top to bottom)
2. Read `docs/HANDOFF.md`
3. Read `state/status.yaml` for current state
4. Read `docs/FAILED_APPROACHES.md` before any code work
5. Open the role file matching your assigned task in `.openclaw/agents/`

## Hooks

If a notification sound is desired, add this to `~/.claude/settings.json`:

```json
{
  "hooks": {
    "Notification": [
      {
        "matcher": "",
        "hooks": [{"type": "command", "command": "afplay /System/Library/Sounds/Submarine.aiff"}]
      }
    ]
  }
}
```
