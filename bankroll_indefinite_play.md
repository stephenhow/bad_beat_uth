# Bankroll for Indefinite Play

How large a bankroll does a player need to play Bad Beat Ultimate Texas Hold'em indefinitely at $100 Antes
with a 99% chance of never busting out?

**Answer: about 1,735 units, or $173,500 at $100 Antes.**

## Assumptions
- Game: the bad-beat version (ENABLE_BAD_BEAT true, PREFLOP_PLAY_X 4), pay table as in uth.cpp.
- Strategy: optimal play on every hand (raise 4x preflop, bet 2x on the flop, or bet 1x/fold on the river, as computed in bb_uth.sqlite3).
- Bets: a flat Ante of 1 unit ($100) and a Blind of 1 unit every hand, forever.
- Busting out: the bankroll falls below 6 units ($600), the most a single hand can lose (1 Ante + 1 Blind + 4 Play).
  Below that, the player can no longer play a hand with the optimal strategy.
- Payouts are uncapped. At $100 Antes this matches the Wôlinak table: its 50,000 $ maximum payout equals 500 to 1 on a $100 Blind.

## Per-hand result
From bb4x_uth.csv (the per-hand outcome distribution under optimal play, 27,813,810,024,000 hands):

| per hand (units)   | value     |
|--------------------|-----------|
| mean (EV)          | +0.029273 |
| variance           | 24.777    |
| standard deviation | 4.978     |

The positive mean is what makes indefinite play survivable: a large enough bankroll drifts upward faster than its swings can wipe it out.

## Method
The bankroll is a Markov chain on a half-unit grid (the 1.5 to 1 flush pays leave half units), with one step per hand,
using the 38 distinct net results of bb4x_uth.csv and their probabilities.
The probability of ever busting out from bankroll b, psi(b), satisfies

    psi(b) = sum over results j of P(j) * psi(b + j),   psi = 1 below 6 units,

which was solved by Gauss-Seidel iteration with an absorbing upper barrier at 6,000 units, far enough away
that the result equals indefinite play to the precision shown.

Cross-check: the Lundberg bound psi(b) <= exp(-R * (b - 6)), where R solves E[exp(-R X)] = 1 for the per-hand
result X, gives R = 0.002660 per unit. It agrees with the exact solution to within 1%; the exact risk is slightly
lower because a bust usually overshoots below the threshold rather than landing on it.
(The diffusion approximation 2 * mean / variance = 0.002363 understates R, because the jackpots skew results upward.)

To reproduce (about 3 minutes):

    node analysis/ruin_inf.js bb4x_uth.csv

## Results

| chance of ever busting out | bankroll (units) | at $100 Antes |
|----------------------------|------------------|---------------|
| 26.7%                      | 500              | $50,000       |
| 7.1%                       | 1,000            | $100,000      |
| 5%                         | 1,130            | $113,000      |
| 1.9%                       | 1,500            | $150,000      |
| **1%**                     | **1,735**        | **$173,500**  |
| 0.5%                       | 2,000            | $200,000      |
| 0.1%                       | 2,601            | $260,100      |

Ruin probability falls exponentially with bankroll: each additional 865 units ($86,500) cuts it by a factor of 10.

## Caveats
- **Bad-beat game only.** The standard game has a negative edge (-0.021850 per hand), so indefinite play busts out
  with certainty, whatever the bankroll.
- **Perfect play.** Mistakes lower the edge, and the required bankroll grows roughly as 1 / edge.
- **Larger Antes.** Above $100, the 50,000 $ cap cuts the ~500 to 1 jackpots, which supply much of the edge;
  the required bankroll (in units) would rise sharply.
- **No time dimension.** This is the chance of ever busting out. Even with a 99% chance of surviving, expect long stretches
  of being down tens of thousands of dollars.
