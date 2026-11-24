# Contract negotiation (planned and built 2026-11-17)

VISION.md: "Contracts pay a fee, an advance and salvage rights, negotiated in
the House office as MW1 does it." MW1's rules are the port's
`MW_MAIN_081B_CONTRACTS.c` (read as reference, not copied), numbers checked
against the player's MW_MAIN.EXE.

## MW1's rules

**The offer** (already ours, `make_contract`): a payment ceiling rolled from
the force and the House; the opening payment a quarter of it (in 10k steps).
Salvage and advance ceilings are the House's base (DS:4B5F, DS:4B69) shifted
by the standing ((favor - grievance) x 4, clamped to ±10); each opens at a
quarter of its ceiling.

**The request.** The player sets any of three terms: payment in 10k steps
(0..9,990k), salvage and advance in 1% steps (0..100%). Terms left alone
aren't asked about. Then SUBMIT.

**The House's answer** (MwNegotiateAndAcceptContractContinuation), term by
term, payment, salvage, advance:
- Asked for no more than the offer: granted (asking less lowers the offer).
- Asked for more: the House concedes a roll of 0..half the room left
  (ceiling - offer) on top of the offer (payment rounded down to 10k). If
  that reaches the request, it's granted; if not, the offer rises to the
  concession and that term failed.
- **Once a term fails, the later terms are not granted even when their roll
  would cover them** (their offer stays, unless their own roll failed and
  moved it). Payment failing blocks salvage and advance; a quirk of MW1's,
  kept.
- All asked terms granted: the contract is **accepted** at once on those
  terms (no separate ACCEPT after a winning SUBMIT).
- Otherwise the counter-offer stands and the request is cleared.

So each round can at best halve the room to the ceiling, and asking for the
ceiling itself is only granted when the roll is the maximum.

**Patience.** Each world rolls the House's patience on arrival: 0..2 plus
the House's base (DS:11A3: Kurita 3, Steiner 0, Marik 4, Liao 5, Davion 0).
Each failed round counts; reaching the patience with a failed round ends
negotiation: "negotiations terminated" (DS:4C25) and **every offer on the
world is withdrawn** until the contracts refresh (travel). **Steiner and
Davion never break off** (their count can't trigger). The count restarts per
offer.

**The standing** changes only the salvage and advance ceilings (and so their
openings): (favor - grievance) x 4, clamped to ±10 percentage points.
Payment isn't touched by standing. And
at 8+ grievance (DS: HOUSE_GRIEVANCE_REFUSAL) the House refuses to offer
anything (`houseContractRefusalText`); ours doesn't yet: the office adds it.

## Ours

**Where:** the House office, the hub's greyed button made live (not a step
between ACCEPT and the briefing). The hub's offer panel keeps ACCEPT and
DECLINE for taking the offer as it stands; NEGOTIATE (or the office button)
opens the office screen on the template:

- The offer: the employer, the target, the map, the force (as the hub says it).
- Three rows: PAYMENT, SALVAGE, ADVANCE, each the House's current offer and
  the request beside it, with − / + buttons (and the keyboard's up/down on
  the selected row, as MW1). A row untouched isn't requested.
- SUBMIT: the House's answer line ("The House agrees." / "Their counter:
  ..." / "Negotiations terminated."). Granted: accepted, back to the hub
  with the contract (as ACCEPT does now: the advance paid).
- ACCEPT (the current counter as it stands), BACK (to the hub, the counter
  kept; the patience count kept for that offer too: leaving doesn't reset
  it, as MW1's EXIT doesn't).
- Patience shown? MW1 doesn't show it. Question for the owner (below).

**Data:** the Contract gains `salvage_ceiling`, `advance_ceiling`,
`attempts`; the Company (or the world) `patience` rolled on arrival and
`offers_withdrawn` until the next refresh. Save version 7 (a version 6 save
reads with ceilings recomputed from the House and standing, attempts 0,
patience rolled).

**Code:** `mw1_contracts` gains `negotiate(Contract&, Request, Rng&,
patience) -> Answer` (pure, MW1's rule, unit-tested against hand-worked
rolls); `src/mw1_office.h/.cpp` the screen; `data/mw1office.zrd` its layout.

**Tests:** the rule (granted at or under the offer; a concession short;
the quirk: payment failing blocks a coverable salvage; asking less lowers;
Steiner/Davion never terminate; Kurita's patience ends it and withdraws the
offers until travel); the standing moving the salvage/advance openings; the
save round-trip and the version-6 read; headless: office from the hub,
a request granted (the hub shows the contract on the new terms, the advance
paid at the negotiated percent), terminated (the hub with no offer, the
office greyed until travel). A --shot of the office.

## Built (2026-11-17)

As planned, with these choices:

- The rule is `negotiate()` in `src/mw1_campaign.h`; the screen `src/mw1_office.h`,
  `data/mw1office.zrd`; the hub's OFFICE button opens it (greyed without an
  offer, and while AFTER ACTION waits).
- **An agreed round is the acceptance**, as MW1's: the office goes straight
  to the briefing, as the hub's ACCEPT does (our advance is paid at the first
  launch, as before). ACCEPT in the office takes the counter as it stands.
- Every asked term is rolled; the quirk only stops later grants (MW1's jumps
  skip the grant, not the roll).
- An offer above its ceiling (only an old save could make one) concedes
  nothing, where MW1's word arithmetic would wrap.
- MW1's words (terminated, refused, accepted) are read from the player's exe
  (DS:4C25, DS:4C87, DS:4C6C); ours stand in without it.
- **The stuck guard** (`stranded`, the hub's and the travel screen's
  message) counts a House that refuses the company like a world with no
  office, and a broken-off negotiation like no contracts here: "You can't
  afford to leave, and the House has broken off negotiations." The way out
  is the cheapest trip to a world whose House office would deal with you;
  none at all says so.
- Save version 7: the world's patience and withdrawal, the offer's salvage
  and advance ceilings and its rounds. A version 6 save reads with the
  House's middle patience and the ceilings from the House and the standing.

## Questions for the owner (provisional answers: mw1 lead 2026-11-17)

Each is one constant or flag to change.

1. **The patience:** hidden; a hint line ("The House's liaison is losing
   patience.") after the round before the last (`last_round`).
2. **The ceilings:** hidden, as MW1.
3. **MW1's quirk:** kept.
4. **Termination:** withdraws the world's offers until travel (`withdrawn`).
5. **Salvage %:** scales the cash only; MW3's items are always taken.
6. **The refusal at 8+ grievances** (`kGrievanceRefusal`): yes, and the
   stuck guard counts it.

## The questions as first asked

1. **Show the House's patience?** MW1 hid it. Hidden keeps MW1's tension; a
   hint ("The liaison is losing patience") after the second-last round is
   kinder. Suggest: the hint line, no number.
2. **Show the ceilings?** MW1 never did. Suggest: hidden.
3. **Keep MW1's quirk** (a failed payment blocks salvage and advance that
   would have been granted)? Suggest: keep; it's MW1's balance.
4. **Termination withdraws all offers on the world** until travel: keep?
   Today we hold one offer at a time, so it means no contract here until
   you leave. Suggest: keep.
5. **Salvage % meets MW3's salvage.** VISION says you get both cash and
   items; the negotiated % scales the cash (as now). Should it also gate the
   item salvage (e.g. 0% salvage: no items)? Suggest: cash only, items
   always (VISION's "taking items never reduces the cash").
6. **Grievance refusal** (8+ against the House): the office shows MW1's
   refusal and offers nothing. Not in ours yet; confirm it's wanted.

**Later (mw1 lead 2026-11-17):** the office is a place, as MW1's. It opens
wherever the world has a House office, with or without an offer. Without
one, it says "No contract on offer.", shows why in the terms' place (MW1's
broken-off or refusal words), and greys SUBMIT, ACCEPT and the terms'
arrows. Its story events (0x08 on Galedon V) fire either way.
