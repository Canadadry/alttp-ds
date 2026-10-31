# TODO

- Randomizer hints and texts: telepathic tiles, Sahasrahla, pedestal, Ether tablet, tavern.
  VT draws them after the fill; `tests/rando/oracle/vt.patch` comments them out and the C port
  stops at the `fill` stage. Needs: re-enable them in the oracle, regenerate the goldens in their
  own commit, port the hint generation to C, show them in game.
