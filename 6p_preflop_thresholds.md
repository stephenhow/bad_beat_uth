# Overview
This document describes the experiment for determining the effect of 10 other known cards ("others") on the 4x/check preflop decision of player's hole cards.

The game is standard Ultimate Texas Holdem (i.e., no bad beat on the blind).

A number of Monte Carlo dealt hands is evaluated to make the 4x/check determination for each (playerHole cards, number of copies) experiment point. 

results: 6p_preflop_thresholds.sqlite3.gz

# Methodology
copies: defined as the total number of rank copies of either playerHole card found in others
The UltimateTexasHoldem::getStats(playerHole, board, others, &stats) will be used to evaluate the optimal results for the Monte Carlo hands meeting the requirements of playerHole rank copies in others.
For each (playerHole, copies) data point in question, run a sufficient number of Monte Carlo dealt hands to determine whether the hand should be 4x'ed or check.
## Parallelism
The Monte Carlo data is processed in batches with addToDatabase(db, (rows = # instance threads), playerHole, 6, copies).
This adds (# instance threads) new rows to the results db for the (playerHole, copies) data point.
Post-processing the results database will be able select rows by (playerHole, copies).

From either running totals, or re-scanning the results database for (playerHole, copies) data points, the average difference of (EV4x - EV0x) will determine when enough Monte Carlo samples have been gathered to determine the 4x/check decision.
If more data points are needed, call addToDatabase again with the same parameters, up to a limit of 5 batches.

# Data Points
## (PlayerHole, copies) Datapoints
With no information of others, the 4x raising thresholds are K5o and higher, and suited King, Q8o and higher, Q6s and higher, JTo, J8s or higher, any Ace, any pair 3's or higher.
The experiments are organized by the rank of the high card of playerHole.
The testing sequences below are organized by high card of the playerHole.  When advancing through the sequence of playerHoles, the low holecard is incremented.
Each datapoint in the series should begin testing with the maximum allowable copies of the previous playerHole in the series.  This is because the hands are ordered in ascending strength.
For the first playerHole in a series, begin testing with 0 copies.

max_copies: 4
only test up to max_copies, because the probabily of more copies than this in others in very low

### Ace
#### A-suited
Test playerHoles (A2s, A3s, A4s ... AKs).  Start with A2s. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series, starting with the maximum allowable copies of the preceeding hand in the series (or 0 if it's the first), until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
#### A-offsuit
Test playerHoles (A2o, A3o, A4o ... AKo).  Start with A2o. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
### King
#### K-suited
Test playerHoles (K2s, K3s, K4s ... KQs).  Start with K2s. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
#### K-offsuit
Test playerHoles (K2o, K3o, K4o ... KQo).  Start with K2o. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
### Queen
#### Q-suited
Test playerHoles (Q2s, Q3s, Q4s ... QJs).  Start with Q2s. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
#### Q-offsuit
Test playerHoles (Q5o, Q6o, Q7o ... QJo).  Start with Q5o. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
### Jack
#### J-suited
Test playerHoles (J6s, J7s, J8s, J9s, JTs).  Start with J6s. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
#### J-offsuit
Test playerHoles (J7o, J8o, J9o, JTo).  Start with J7o. Find the maximum number of rank copies that still allow for a 4x wager for a given playerHole, then stop for that playerHole.
Test the next playerHole in the series until you reach a hand that is still 4x with max_copies, then stop the testing of the series.
### Ten
#### Ten-suited
See if T9s works for 0, or 1 copies.
See if T8s works for 0, or 1 copies.
#### Ten-offsuit
See if T9o works for 0, or 1 copies.
See if T8o works for 0, or 1 copies.
### Nine
#### Nine-suited
See if 98s works for 0, or 1 copies.

### Pairs
For a paired playerHole, only 2 cards of its rank remain, so copies can be 0, 1 or 2 (max_copies for pairs: 2).
Test playerHoles (22, 33, 44, 55, 66), starting with 22 at 0 copies; each pair starts at the maximum allowable copies of the preceding pair.
Test all five pairs (no early stop), to confirm that 55 and higher are 4x even with 2 copies.

## Probabilities
The probability of n total rank copies of a non-paired playerHole in 10 other cards:
 Pr(0 copies) = C(6, 0) * C(44, 10) / C(50, 10) = 2,481,256,778 / 10,272,278,170 = 0.241549 (24.1549%)
 Pr(1 copies) = C(6, 1) * C(44,9)   / C(50,10) =  4,253,583,048 / 10,272,278,170 = 0.414084 (41.4084%)
 Pr(2 copies) = C(6, 2) * C(44,8)   / C(50,10) =  2,658,489,405 / 10,272,278,170 = 0.258802 (25.8802%)
 Pr(3 copies) = C(6, 3) * C(44,7)   / C(50,10) =    766,411,360 / 10,272,278,170 = 0.074610 ( 7.4610%)
 Pr(4 copies) = C(6, 4) * C(44,6)   / C(50,10) =    105,885,780 / 10,272,278,170 = 0.010308 ( 1.0308%)
 Pr(5 copies) = C(6, 5) * C(44,5)   / C(50,10) =      6,516,048 / 10,272,278,170 = 0.000634 ( 0.0634%)
 Pr(6 copies) = C(6, 6) * C(44,4)   / C(50,10) =        135,751 / 10,272,278,170 = 0.000013 ( 0.0013%)
 The seven probabilities add up to exactly 1 (the numerators total 10,272,278,170 = C(50,10)).

The probability of n total rank copies of a paired playerHole in 10 other cards:
 Pr(0 copies) = C(2, 0) * C(48,10)   / C(50,10) = 6,540,715,896 / 10,272,278,170 = 0.636735 (63.6735%)
 Pr(1 copies) = C(2, 1) * C(48,9)    / C(50,10) = 3,354,213,280 / 10,272,278,170 = 0.326531 (32.6531%)
 Pr(2 copies) = C(2, 2) * C(48,8)    / C(50,10) =   377,348,994 / 10,272,278,170 = 0.036735 ( 3.6735%)
 The three probabilities add up to exactly 1 (the numerators total 10,272,278,170 = C(50,10)).

# Results
Here are the rules for 4x'ing playerHole cards, depending on the number of copies of the playerHole ranks among the 10 known cards (the 5 other players' hole cards): raise 4x when the copies are at most the max shown; otherwise check.

| playerHole | max copies for 4x | notes                                                                                                             |
|------------|-------------------|-------------------------------------------------------------------------------------------------------------------|
| A2s        | 2                 |                                                                                                                   |
| A3s        | 3                 |                                                                                                                   |
| A4s        | 3                 |                                                                                                                   |
| A5s        | 4                 | still 4x with 4 copies (max_copies), so the A-suited series ends here                                             |
| A6s        | 4                 | 4 copies is a close call; tested before max_copies = 4 was adopted; the series ends at A5s under the current rule |
| A7s        | 4                 | tested before max_copies = 4 was adopted; the series ends at A5s under the current rule                           |
| A8s        | 4                 | tested before max_copies = 4 was adopted; the series ends at A5s under the current rule                           |
| A9s        | 4                 | tested before max_copies = 4 was adopted; the series ends at A5s under the current rule                           |
| ATs        | 4                 | tested before max_copies = 4 was adopted; the series ends at A5s under the current rule                           |
| A2o        | 2                 | 2 copies is a close call                                                                                          |
| A3o        | 2                 |                                                                                                                   |
| A4o        | 2                 |                                                                                                                   |
| A5o        | 3                 |                                                                                                                   |
| A6o        | 3                 |                                                                                                                   |
| A7o        | 3                 |                                                                                                                   |
| A8o        | 3                 |                                                                                                                   |
| A9o        | 3                 |                                                                                                                   |
| ATo        | 4                 | still 4x with 4 copies (max_copies), so the A-offsuit series ends here                                            |
| K2s        | 1                 |                                                                                                                   |
| K3s        | 1                 |                                                                                                                   |
| K4s        | 1                 |                                                                                                                   |
| K5s        | 2                 |                                                                                                                   |
| K6s        | 2                 |                                                                                                                   |
| K7s        | 2                 |                                                                                                                   |
| K8s        | 2                 |                                                                                                                   |
| K9s        | 3                 |                                                                                                                   |
| KTs        | 3                 |                                                                                                                   |
| KJs        | 3                 |                                                                                                                   |
| KQs        | 3                 |                                                                                                                   |
| K2o        | 0                 |                                                                                                                   |
| K3o        | 0                 |                                                                                                                   |
| K4o        | 1                 |                                                                                                                   |
| K5o        | 1                 |                                                                                                                   |
| K6o        | 1                 |                                                                                                                   |
| K7o        | 1                 | 2 copies is a close call                                                                                          |
| K8o        | 2                 |                                                                                                                   |
| K9o        | 2                 |                                                                                                                   |
| KTo        | 2                 | 3 copies is a close call                                                                                          |
| KJo        | 3                 | 3 copies is a close call                                                                                          |
| KQo        | 3                 |                                                                                                                   |
| Q2s        | 0                 |                                                                                                                   |
| Q3s        | 0                 |                                                                                                                   |
| Q4s        | 0                 |                                                                                                                   |
| Q5s        | 1                 | 1 copy is a close call                                                                                            |
| Q6s        | 1                 |                                                                                                                   |
| Q7s        | 1                 |                                                                                                                   |
| Q8s        | 2                 | 2 copies is a close call                                                                                          |
| Q9s        | 2                 |                                                                                                                   |
| QTs        | 2                 |                                                                                                                   |
| QJs        | 2                 |                                                                                                                   |
| Q5o        | 0                 |                                                                                                                   |
| Q6o        | 0                 |                                                                                                                   |
| Q7o        | 0                 |                                                                                                                   |
| Q8o        | 1                 |                                                                                                                   |
| Q9o        | 1                 |                                                                                                                   |
| QTo        | 2                 |                                                                                                                   |
| QJo        | 2                 |                                                                                                                   |
| J6s        | 0                 |                                                                                                                   |
| J7s        | 0                 |                                                                                                                   |
| J8s        | 1                 |                                                                                                                   |
| J9s        | 1                 |                                                                                                                   |
| JTs        | 2                 |                                                                                                                   |
| J7o        | 0                 |                                                                                                                   |
| J8o        | 0                 |                                                                                                                   |
| J9o        | 1                 |                                                                                                                   |
| JTo        | 1                 |                                                                                                                   |
| T9s        | 1                 |                                                                                                                   |
| T8s        | 0                 |                                                                                                                   |
| T7s        | 0                 | tested at 0 copies after the series (basic: check)                                                                |
| T9o        | 0                 |                                                                                                                   |
| T8o        | 0                 |                                                                                                                   |
| 98s        | 0                 |                                                                                                                   |
| 22         | 0                 |                                                                                                                   |
| 33         | 0                 |                                                                                                                   |
| 44         | 1                 |                                                                                                                   |
| 55         | 2                 | 2 copies is a close call; 4x even with both remaining cards of its rank out                                       |
| 66         | 2                 | 4x even with both remaining cards of its rank out                                                                 |

Close calls (still within 3 standard errors after the 5-batch limit, 320 rows; decided by the sign of the mean, effectively toss-ups):
- A6s, 4 copies: 4x ahead by +0.0036 ± 0.0046
- A2o, 2 copies: 4x ahead by +0.0047 ± 0.0031
- K7o, 2 copies: check ahead by -0.0019 ± 0.0032
- KTo, 3 copies: check ahead by -0.0030 ± 0.0042
- KJo, 3 copies: 4x ahead by +0.0109 ± 0.0040
- Q5s, 1 copy: 4x ahead by +0.0153 ± 0.0053
- Q8s, 2 copies: 4x ahead by +0.0060 ± 0.0054
- 55, 2 copies: 4x ahead by +0.0060 ± 0.0083

## Preflop Gain
for all (playerHole, copies) where copies <= 3, and where the optimal preflop decision differs from basic strategy (4x/check based on no other info):

Average gain: mean EV gain per deal (ante units) from making the 6-player decision instead of the basic-strategy one, ± 95% CI.
Frequency: Pr(playerHole) * Pr(copies) = Pr(playerHole, copies); Pr(playerHole) = 4/1326 suited, 12/1326 offsuit, 6/1326 pairs.
Added return: frequency * average gain, ± 95% CI, in ante units per hand dealt. Rows: Monte Carlo rows for the point in the results database.

| playerHole | copies | average gain     | frequency | added return +/- CI   | basic → 6p         | rows |
|------------|--------|------------------|-----------|-----------------------|--------------------|------|
| A2s        |      3 | +0.0106 ± 0.0071 | 0.0002251 | 0.0000024 ± 0.0000016 | 4x → check         |  640 |
| A2o        |      3 | +0.1457 ± 0.0177 | 0.0006752 | 0.0000984 ± 0.0000119 | 4x → check         |   64 |
| A3o        |      3 | +0.0960 ± 0.0143 | 0.0006752 | 0.0000648 ± 0.0000096 | 4x → check         |  128 |
| A4o        |      3 | +0.0450 ± 0.0090 | 0.0006752 | 0.0000304 ± 0.0000061 | 4x → check         |  256 |
| K2s        |      2 | +0.1125 ± 0.0217 | 0.0007807 | 0.0000878 ± 0.0000169 | 4x → check         |   64 |
| K2s        |      3 | +0.3058 ± 0.0232 | 0.0002251 | 0.0000688 ± 0.0000052 | 4x → check         |   64 |
| K3s        |      2 | +0.0916 ± 0.0154 | 0.0007807 | 0.0000715 ± 0.0000120 | 4x → check         |  128 |
| K3s        |      3 | +0.2436 ± 0.0246 | 0.0002251 | 0.0000548 ± 0.0000055 | 4x → check         |   64 |
| K4s        |      2 | +0.0208 ± 0.0072 | 0.0007807 | 0.0000163 ± 0.0000056 | 4x → check         |  640 |
| K4s        |      3 | +0.1799 ± 0.0233 | 0.0002251 | 0.0000405 ± 0.0000052 | 4x → check         |   64 |
| K5s        |      3 | +0.1306 ± 0.0227 | 0.0002251 | 0.0000294 ± 0.0000051 | 4x → check         |   64 |
| K6s        |      3 | +0.0793 ± 0.0150 | 0.0002251 | 0.0000178 ± 0.0000034 | 4x → check         |  128 |
| K7s        |      3 | +0.0615 ± 0.0108 | 0.0002251 | 0.0000138 ± 0.0000024 | 4x → check         |  256 |
| K8s        |      3 | +0.0233 ± 0.0067 | 0.0002251 | 0.0000052 ± 0.0000015 | 4x → check         |  640 |
| K2o        |      0 | +0.0857 ± 0.0125 | 0.0021860 | 0.0001874 ± 0.0000274 | check → 4x         |   64 |
| K3o        |      0 | +0.1177 ± 0.0158 | 0.0021860 | 0.0002573 ± 0.0000346 | check → 4x         |   64 |
| K4o        |      0 | +0.1678 ± 0.0134 | 0.0021860 | 0.0003668 ± 0.0000293 | check → 4x         |   64 |
| K4o        |      1 | +0.0101 ± 0.0050 | 0.0037474 | 0.0000379 ± 0.0000186 | check → 4x         |  640 |
| K5o        |      2 | +0.0929 ± 0.0139 | 0.0023421 | 0.0002176 ± 0.0000326 | 4x → check         |   64 |
| K5o        |      3 | +0.2822 ± 0.0177 | 0.0006752 | 0.0001906 ± 0.0000120 | 4x → check         |   64 |
| K6o        |      2 | +0.0509 ± 0.0090 | 0.0023421 | 0.0001192 ± 0.0000210 | 4x → check         |  192 |
| K6o        |      3 | +0.2369 ± 0.0152 | 0.0006752 | 0.0001599 ± 0.0000103 | 4x → check         |   64 |
| K7o        |      2 | +0.0014 ± 0.0046 | 0.0023421 | 0.0000033 ± 0.0000108 | 4x → check (close) |  640 |
| K7o        |      3 | +0.1921 ± 0.0150 | 0.0006752 | 0.0001297 ± 0.0000101 | 4x → check         |   64 |
| K8o        |      3 | +0.1794 ± 0.0144 | 0.0006752 | 0.0001211 ± 0.0000097 | 4x → check         |   64 |
| K9o        |      3 | +0.0869 ± 0.0157 | 0.0006752 | 0.0000587 ± 0.0000106 | 4x → check         |   64 |
| KTo        |      3 | +0.0053 ± 0.0055 | 0.0006752 | 0.0000036 ± 0.0000037 | 4x → check (close) |  640 |
| Q2s        |      0 | +0.0253 ± 0.0068 | 0.0007287 | 0.0000184 ± 0.0000049 | check → 4x         |  640 |
| Q3s        |      0 | +0.0761 ± 0.0132 | 0.0007287 | 0.0000555 ± 0.0000096 | check → 4x         |  192 |
| Q4s        |      0 | +0.1165 ± 0.0161 | 0.0007287 | 0.0000849 ± 0.0000117 | check → 4x         |  128 |
| Q5s        |      0 | +0.1841 ± 0.0222 | 0.0007287 | 0.0001341 ± 0.0000162 | check → 4x         |   64 |
| Q5s        |      1 | +0.0115 ± 0.0076 | 0.0012491 | 0.0000144 ± 0.0000095 | check → 4x (close) |  640 |
| Q6s        |      2 | +0.1090 ± 0.0158 | 0.0007807 | 0.0000851 ± 0.0000123 | 4x → check         |  128 |
| Q6s        |      3 | +0.2894 ± 0.0241 | 0.0002251 | 0.0000651 ± 0.0000054 | 4x → check         |   64 |
| Q7s        |      2 | +0.0985 ± 0.0174 | 0.0007807 | 0.0000769 ± 0.0000136 | 4x → check         |  128 |
| Q7s        |      3 | +0.2640 ± 0.0254 | 0.0002251 | 0.0000594 ± 0.0000057 | 4x → check         |   64 |
| Q8s        |      3 | +0.2051 ± 0.0270 | 0.0002251 | 0.0000462 ± 0.0000061 | 4x → check         |   64 |
| Q9s        |      3 | +0.1175 ± 0.0173 | 0.0002251 | 0.0000264 ± 0.0000039 | 4x → check         |  128 |
| QTs        |      3 | +0.0435 ± 0.0082 | 0.0002251 | 0.0000098 ± 0.0000019 | 4x → check         |  640 |
| QJs        |      3 | +0.0430 ± 0.0086 | 0.0002251 | 0.0000097 ± 0.0000019 | 4x → check         |  576 |
| Q5o        |      0 | +0.0598 ± 0.0099 | 0.0021860 | 0.0001306 ± 0.0000217 | check → 4x         |  192 |
| Q6o        |      0 | +0.1085 ± 0.0174 | 0.0021860 | 0.0002371 ± 0.0000380 | check → 4x         |   64 |
| Q7o        |      0 | +0.1458 ± 0.0147 | 0.0021860 | 0.0003188 ± 0.0000322 | check → 4x         |   64 |
| Q8o        |      2 | +0.1347 ± 0.0184 | 0.0023421 | 0.0003155 ± 0.0000431 | 4x → check         |   64 |
| Q8o        |      3 | +0.3665 ± 0.0204 | 0.0006752 | 0.0002474 ± 0.0000138 | 4x → check         |   64 |
| Q9o        |      2 | +0.0597 ± 0.0106 | 0.0023421 | 0.0001398 ± 0.0000249 | 4x → check         |  192 |
| Q9o        |      3 | +0.2798 ± 0.0228 | 0.0006752 | 0.0001889 ± 0.0000154 | 4x → check         |   64 |
| QTo        |      3 | +0.1920 ± 0.0209 | 0.0006752 | 0.0001296 ± 0.0000141 | 4x → check         |   64 |
| QJo        |      3 | +0.1929 ± 0.0193 | 0.0006752 | 0.0001302 ± 0.0000131 | 4x → check         |   64 |
| J6s        |      0 | +0.0556 ± 0.0108 | 0.0007287 | 0.0000405 ± 0.0000078 | check → 4x         |  384 |
| J7s        |      0 | +0.1622 ± 0.0242 | 0.0007287 | 0.0001182 ± 0.0000176 | check → 4x         |   64 |
| J8s        |      2 | +0.1343 ± 0.0249 | 0.0007807 | 0.0001048 ± 0.0000194 | 4x → check         |   64 |
| J8s        |      3 | +0.3608 ± 0.0255 | 0.0002251 | 0.0000812 ± 0.0000057 | 4x → check         |   64 |
| J9s        |      2 | +0.0503 ± 0.0098 | 0.0007807 | 0.0000393 ± 0.0000076 | 4x → check         |  512 |
| J9s        |      3 | +0.3162 ± 0.0254 | 0.0002251 | 0.0000712 ± 0.0000057 | 4x → check         |   64 |
| JTs        |      3 | +0.2129 ± 0.0288 | 0.0002251 | 0.0000479 ± 0.0000065 | 4x → check         |   64 |
| J7o        |      0 | +0.0388 ± 0.0073 | 0.0021860 | 0.0000848 ± 0.0000161 | check → 4x         |  448 |
| J8o        |      0 | +0.1087 ± 0.0202 | 0.0021860 | 0.0002377 ± 0.0000441 | check → 4x         |   64 |
| J9o        |      0 | +0.1891 ± 0.0202 | 0.0021860 | 0.0004135 ± 0.0000443 | check → 4x         |   64 |
| J9o        |      1 | +0.0182 ± 0.0066 | 0.0037474 | 0.0000680 ± 0.0000249 | check → 4x         |  640 |
| JTo        |      2 | +0.1020 ± 0.0151 | 0.0023421 | 0.0002388 ± 0.0000353 | 4x → check         |  128 |
| JTo        |      3 | +0.3613 ± 0.0239 | 0.0006752 | 0.0002440 ± 0.0000162 | 4x → check         |   64 |
| T9s        |      0 | +0.1962 ± 0.0249 | 0.0007287 | 0.0001429 ± 0.0000182 | check → 4x         |   64 |
| T9s        |      1 | +0.0284 ± 0.0083 | 0.0012491 | 0.0000355 ± 0.0000104 | check → 4x         |  640 |
| T8s        |      0 | +0.1434 ± 0.0230 | 0.0007287 | 0.0001045 ± 0.0000168 | check → 4x         |   64 |
| T7s        |      0 | +0.0356 ± 0.0085 | 0.0007287 | 0.0000259 ± 0.0000062 | check → 4x         |  640 |
| T9o        |      0 | +0.1004 ± 0.0199 | 0.0021860 | 0.0002194 ± 0.0000434 | check → 4x         |   64 |
| T8o        |      0 | +0.0132 ± 0.0073 | 0.0021860 | 0.0000288 ± 0.0000159 | check → 4x         |  640 |
| 98s        |      0 | +0.0277 ± 0.0093 | 0.0007287 | 0.0000202 ± 0.0000068 | check → 4x         |  640 |
| 22         |      0 | +0.0338 ± 0.0060 | 0.0028812 | 0.0000973 ± 0.0000174 | check → 4x         |  320 |
| 33         |      1 | +0.1301 ± 0.0227 | 0.0014775 | 0.0001923 ± 0.0000336 | 4x → check         |  128 |
| 33         |      2 | +0.4604 ± 0.0297 | 0.0001662 | 0.0000765 ± 0.0000049 | 4x → check         |   64 |
| 44         |      2 | +0.2066 ± 0.0356 | 0.0001662 | 0.0000343 ± 0.0000059 | 4x → check         |   64 |

Total added return (sum over all 73 points): +0.007747 ± 0.000159 ante units per hand dealt (95% CI).
With the standard game's -0.021850 per hand under basic strategy, the 6-player preflop decision alone lifts it to about -0.014103.

How often the 10 known cards change the preflop decision (sum of the frequencies):
- in total: 0.07881 (7.881% of hands dealt)
- basic 4x → check: 0.03461 (3.461%)
- basic check → 4x: 0.04421 (4.421%)
- of which close-call points (K7o 2 copies, KTo 3 copies, Q5s 1 copy, effectively toss-ups): 0.00427 (0.427%)

Notes:
- copies <= 3 only; 4 or more copies (about 1.10% of deals for a non-paired hand) are not counted.
- Points above a hand's cutoff that the threshold search skipped (e.g. K5o with 3 copies) were run for this table; all confirmed check.
- Each point has at least 64 rows, plus batches until its 95% CI was within ±20% of its gain, up to 10 batches (640 rows); points at 640 rows have wider intervals but small gains, so they hardly affect the total.
- T7s was tested at 0 copies after the series (it switches basic check → 4x); it is included above.
- Covers the hands in the series above plus T7s, which the spec takes to include every hand whose decision can change.

## Total Gain
Total EV of the standard game with 6 players when the player knows the other 5 players' hole cards (10 cards), using every decision the known cards allow:
isPlay4x(playerHole, others) preflop, and getStats(playerHole, flop, others) for the optimal flop 2x/check and river 1x/fold decisions.

Method: simPostflop(6, 2000000), a Monte Carlo over 2,000,000 random deals (2 hole cards, 10 known cards, 3-card flop). Each deal is played twice:
with the known cards as above, and with no known cards (basic strategy and getStats(playerHole, flop, {})). The per-deal difference is the gain
from knowing the cards; both plays share the deal's luck, so the gain converges far faster (per-deal SD about 0.34) than the EV itself (SD about 1.7).
Total EV = the exact no-information EV (-0.0218497, from uth.sqlite3) + mean gain.

| measure (ante units per hand)                      | 2,000,000 hands      |
|----------------------------------------------------|----------------------|
| total EV with 10 known cards                       | -0.010748 ± 0.000529 |
| gain over basic strategy                           | +0.011102 ± 0.000529 |
| direct EV with known cards (check)                 | -0.00956 ± 0.00257   |
| EV with no known cards (check; exact -0.0218497)   | -0.02066 ± 0.00252   |
| hands raised 4x preflop                            | 38.33%               |
| hands whose preflop decision differs from basic    | 8.09%                |

All ± are 95% confidence intervals. The two checks agree with the total EV and with the exact no-information EV.

Findings:
- Knowing the other 5 players' hole cards cuts the house edge about in half, from 2.18% to 1.07% of the Ante, but the game stays negative for the player
  (the whole 95% interval is below -0.0102).
- The preflop decision changes on 8.09% of hands, a little more than the 7.81% in Preflop Gain because the simulation also includes 4 or more copies.
- The simulation ran before T7s was found to switch to 4x with 0 copies, so it played T7s as check there. T7s adds only about +0.000026
  per hand (Preflop Gain), far inside the ±0.000529 interval, so the total EV above stands.
- The Preflop Gain table's +0.0077 (copies <= 3) is roughly 70% of the total gain; the rest comes from the flop/river decisions and the rarer 4+ copy hands.
  The simulation doesn't split the gain into preflop and postflop parts: comparing the checked hands with and without the known cards is biased,
  because which hands check depends on the known cards themselves.

Run: AWS c8g.16xlarge, 64 threads, about 1,070 paired hands per second (31 minutes), ENABLE_BAD_BEAT false, PREFLOP_PLAY_X 4,
code from commit 3e5055b plus the working-tree isPlay4x() charts and simPostflop().
