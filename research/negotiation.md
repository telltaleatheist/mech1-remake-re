# Contract negotiation: the plan (2026-11-17, for the mw1 lead's review)

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
- Patience shown? MW1 doesn't show it. Question for Owen (below).

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

## Questions for Owen (VISION leaves open)

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
