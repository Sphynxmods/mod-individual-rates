# mod-individual-rates

Account-first, player-controlled progression and reward rates for
[AzerothCore](https://www.azerothcore.org/) 3.3.5a.

This module replaces the older individual XP-only approach with a configurable
`.rate` system for XP, XP level brackets, skill gains, reputation, PvP rewards,
drop chances, loot money, account-wide rates, optional character overrides,
account unlocks, max-level caps, and XP locking.

## Features

- Account-wide rates by default.
- Optional character-specific overrides that take priority over account rates.
- Category commands such as `.rate set xp 5` and individual commands such as
  `.rate set xp.kill 2`.
- Account unlock requirements based on the highest-level character on the
  account.
- Optional per-character max-level caps, useful for reducing drop or XP rates
  once a character reaches a configured level.
- Decimal rates such as `0.5`, `1.3`, and `1.5`.
- XP lock with `.rate lock` and `.rate unlock`.
- Migration support for old `mod-individual-xp` character data.
- Server-owner controls for every category and every individual rate.

## Supported Rates

### Experience

- `xp.kill`
- `xp.quest`
- `xp.quest.df`
- `xp.explore`
- `xp.battlegroundbonus`
- `xp.bg.av`
- `xp.bg.wsg`
- `xp.bg.ab`
- `xp.bg.eots`
- `xp.bg.sota`
- `xp.bg.ic`

### XP Level Brackets

These multiply with the XP source rates above.

- `xp.level.1-9`
- `xp.level.10-19`
- `xp.level.20-29`
- `xp.level.30-39`
- `xp.level.40-49`
- `xp.level.50-59`
- `xp.level.60-69`
- `xp.level.70-79`
- `xp.level.80`

### Skill Gains

- `skill.weapon`
- `skill.defense`
- `skill.gathering`
- `skill.crafting`
- `skill.prof.primary`
- `skill.prof.secondary`
- `skill.gathering.herbalism`
- `skill.gathering.mining`
- `skill.gathering.skinning`
- `skill.gathering.fishing`

### Reputation and PvP

- `rep.global`
- `rep.kill`
- `rep.quest`
- `rep.other`
- `honor`
- `arenapoints`

### Drops and Money

- `drop.chance`
- `drop.poor`
- `drop.normal`
- `drop.uncommon`
- `drop.rare`
- `drop.epic`
- `drop.legendary`
- `drop.artifact`
- `drop.referenced`
- `drop.questitem`
- `drop.recipe`
- `drop.pet`
- `drop.mount`
- `drop.money`

## Important World Config Note

This module multiplies values after AzerothCore worldserver rates are already
applied. For predictable player-facing rates, set the matching `worldserver.conf`
rates to `1` and let this module control the personal multipliers.

Example: if `worldserver.conf` has `Rate.XP.Kill = 2` and a player sets
`xp.kill` to `3`, the final kill XP is effectively `6x`.

## Installation

1. Clone this repository into your AzerothCore `modules/` directory:

   ```bash
   cd /path/to/azerothcore/modules
   git clone <repository-url> mod-individual-rates
   ```

2. Re-run CMake for your AzerothCore build.

3. Rebuild `worldserver`.

4. Copy the config template:

   ```bash
   cp modules/mod-individual-rates/conf/individual_rates.conf.dist \
      conf/individual_rates.conf
   ```

5. Review `conf/individual_rates.conf` before starting the worldserver.

The module ships character and world database SQL under `data/sql/`. AzerothCore
will apply module SQL through the normal database updater flow.

## Configuration

All settings live in:

```text
conf/individual_rates.conf.dist
```

The distributed config is heavily commented and is intended to be the main
reference. The most important top-level settings are:

```ini
IndividualRates.Enabled = 1
IndividualRates.AccountRates.Enabled = 1
IndividualRates.CharacterRates.Enabled = 0
IndividualRates.SetCommand.AccountDefault = 1
IndividualRates.AccountUnlocks.Enabled = 0
```

### Account Rates vs Character Rates

By default, `.rate set` writes account-wide rates:

```text
.rate set xp.kill 2
```

That value applies to every character on the account.

If character rates are enabled, a player can create a character-specific
override:

```text
.rate character set xp.kill 0.5
```

Character overrides have higher priority than account rates. To remove a
character override and return to the account value:

```text
.rate character reset xp.kill
```

To remove both account and character overrides and return to the server default:

```text
.rate reset xp.kill
```

### Account Unlocks

`IndividualRates.AccountUnlocks.Enabled` is a global master switch for all
category and individual `Unlock.*` settings. It exists so server owners can
configure unlock rules per rate but enable or disable the whole unlock system in
one place.

Simple unlock example:

```ini
IndividualRates.AccountUnlocks.Enabled = 1
IndividualRates.Rate.XP.Kill.Unlock.Enabled = 1
IndividualRates.Rate.XP.Kill.Unlock.Level = 80
IndividualRates.Rate.XP.Kill.Unlock.Steps =
```

With that setup, a player can set `xp.kill` above the default only after one
character on the account reaches level 80. Once unlocked, every character on the
account can use the higher rate.

Step unlock example:

```ini
IndividualRates.Rate.XP.Kill.Unlock.Steps = 2:10,3:20,4:30,5:40,6:50,7:60,8:70,9:80
```

Each step is `RateX:UnlockLevel`. If one character on the account reaches the
unlock level, that rate becomes available to the whole account. If
`Unlock.Steps` is set, it takes priority over `Unlock.Level`.

### MaxLevel Caps

MaxLevel caps are separate from account unlocks and use the current character's
level.

Default drop cap example:

```ini
IndividualRates.Category.Drops.MaxLevel.Enabled = 1
IndividualRates.Category.Drops.MaxLevel.Level = 70
IndividualRates.Category.Drops.MaxLevel.MaxRate = 3
```

When enabled, the max drop rate that can apply to a level 70+ character is `x3`.
If the saved account or character value is higher, the module automatically
clamps the active value to `x3`. Values already at or below `x3` keep their
original value.

## Player Commands

| Command | Description |
|---|---|
| `.rate help` | Show command help. |
| `.rate view` | Show currently changeable rates only. |
| `.rate list` | Show category keys and rate keys. |
| `.rate set <key\|category> <value>` | Set the server default target, account by default. |
| `.rate setall <category> <value>` | Set every enabled rate in a category. |
| `.rate account set <key\|category> <value>` | Set account-wide rates directly. |
| `.rate character set <key\|category> <value>` | Set character-specific overrides. |
| `.rate character reset <key\|category>` | Remove character overrides and fall back to account/default. |
| `.rate reset <key\|category>` | Remove account and character overrides and use server defaults. |
| `.rate default <key>` | Reset one rate for the server default target. |
| `.rate enable <key>` / `.rate disable <key>` | Toggle one rate for the server default target. |
| `.rate lock` / `.rate unlock` | Lock or unlock XP gain. |

Category keys:

```text
xp
xplevel
combat
professions
reputation
pvp
drops
money
```

## Examples

Set all XP source rates to `x5` for the account:

```text
.rate set xp 5
```

Then lower only Alterac Valley kill XP to `x2`:

```text
.rate set xp.bg.av 2
```

Set all XP level brackets to half speed:

```text
.rate set xplevel 0.5
```

Create a character-only drop override:

```text
.rate character set drops 2
```

Return that character to account drop rates:

```text
.rate character reset drops
```

Return the account and character to server defaults:

```text
.rate reset drops
```

## Migration from mod-individual-xp

This module can import old `mod-individual-xp` values from the `individualxp`
character table when:

```ini
IndividualRates.MigrateIndividualXp.Enabled = 1
IndividualRates.CharacterRates.Enabled = 1
```

The old XP rate is copied into supported XP source rates for that character.
XP level bracket rates stay at their configured defaults because they multiply
with the XP source rates.

Existing `individual_rates` rows are never overwritten.

## License

GNU Affero General Public License v3.0
