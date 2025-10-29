# Analysis of Fritz 20 Bala 1+2 Games (29 Oct 2025)

## Overview
Two rapid games (1 minute + 2 seconds increment) against Fritz 20 highlight several recurring issues in the SirioC configuration: shallow search depth, low node throughput, and evaluation swings that miss tactical resources. Fritz 20 ran on the same hardware and achieved substantially deeper plies and higher kN/s in the critical positions. This document summarizes the problems and suggests mitigations.

## Hardware and Search Metrics
* Identical GUI conditions: both engines ran inside the Fritz 20 GUI with 4 cores, 1024 MB hash, 120 ms overhead, and the same opening suite (games repeated with colors reversed).
* The Fritz interface exposes the Syzygy tablebases to both engines, so the absence of hits is specific to SirioC’s probing.
* Game 1 (SirioC as Black): average depth 6.6 plies, 1.467 kN/s; Fritz 20 reached 23.5 plies at 2.102 kN/s.
* Game 2 (SirioC as White): average depth 7.0 plies, 1.415 kN/s; Fritz 20 achieved 21.6 plies at 2.234 kN/s.
* SirioC lagged by ~1.5–2x in node throughput despite identical hardware, indicating suboptimal threading or pruning configuration.

## Game 1 Highlights (SirioC Black)
1. Opening: The line 2...b5?! allowed 3.e4 Nxe4 4.Bxb5. SirioC evaluated 4...e6 with only 6 plies, missing that the queenside is collapsing.
2. Move 10...Qb8?! (depth 10, eval +0.34) ignored the central strike 11.d5, which Fritz exploited. Recommended: prioritize ...d6 or ...c6 to challenge the center.
3. Move 16...Nxd5? (depth 7, eval +0.02) overlooked the follow-up 18.Nxg5 and 19.Qd3, leading to mating threats. Deeper search would have surfaced 16...Qd8 or 16...g6.
4. Defensive resource 19...g6 was rejected at depth 1 with -0.97. Even though the Fritz GUI grants tablebase access, SirioC never reported a Syzygy hit, so the engine failed to leverage the endgame database to flag the forced mate. Horizon effect due to shallow search caused immediate mate.

### Action Items
* **Opening book**: Remove or deprioritize 2...b5 in d4 openings unless backed by theory.
* **Search depth**: Increase minimum depth near king attacks (extensions on queen sorties and opposite-side castling threats).
* **Tactical pruning**: Review null-move and late-move pruning parameters; current settings appear to prune critical defensive moves like 19...g6.

## Game 2 Highlights (SirioC White)
1. After 11.Re2, SirioC spent tempo preparing cxb5 structures instead of immediate central expansion (11.cxb5 or 11.d5). Evaluation at depth 23 already favored Black, indicating mis-evaluation of space advantage.
2. The sequence 18...c5! and 20...d5! showed Fritz’s superior dynamic evaluation; SirioC’s depth 2–5 search failed to anticipate the pawn breaks.
3. Critical blunder 21.Qg3?? evaluated +1.45 at depth 2 but allowed 21...Qb6! (depth 23, -3.87). Need higher required depth before accepting queen moves that leave back-rank weak.
4. Endgame transition from move 24 onward: SirioC allowed simplification into inferior minor-piece endgame; low kN/s prevented identifying defensive 25.Rf4 or 27.Qd1. Move 33...e3! and 34...e2! were unstoppable once the queen was misplaced.

### Action Items
* **Depth management**: Implement iterative deepening time-slice to guarantee at least depth 12 before committing to major decisions (queen moves, pawn breaks).
* **Selective extensions**: Extend search when opponent pawn storms occur (e.g., ...c5, ...d5) to evaluate resulting open files accurately.
* **Evaluation tuning**: Revisit king safety terms; SirioC underestimated the danger along the c-file and dark squares after piece trades.

## Engine Configuration Recommendations
1. **Time Management**
   * Increase move-overhead to avoid flagging yet ensure more time for midgame calculations.
   * Allocate time based on volatility of evaluation; spike beyond 100 cp should trigger deeper search.
2. **Parallelism**
   * Verify that all logical cores are used. Measure threads via UCI `perft` or profiling; adjust split depth or thread affinity.
3. **Hash Tables**
   * Ensure hash size is at least 128 MB even in bullet; collisions at lower sizes reduce effective depth.
4. **Tablebase Usage**
   * Diagnose why SirioC fails to register Syzygy hits inside the Fritz 20 GUI despite shared configuration. Verify `SyzygyPath`
     parsing and probe code so defensive resources such as 19...g6 are reinforced instead of being pruned.

## Testing Plan
* Run self-play matches at 1+2 against Fritz 20 after adjusting search parameters to measure depth and kN/s improvement.
* Execute tactical suite focusing on forced-mate detection to validate extensions prevent horizon issues.
* Use profiling to compare node throughput before/after threading tweaks.

