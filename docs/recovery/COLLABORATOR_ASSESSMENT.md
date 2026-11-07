# Constructive assessment for discussion

The visual direction is worth keeping. Adaptive widescreen, a cleaner HUD, steadier geometry and better performance are sensible goals for R4. Some of the work already improved the shared engine, and the retained measurements and failed-gate reports make recovery much easier. This is not a recommendation to throw everything away.

The main problem is that the engine fork became too large and too far behind its foundation to judge changes independently. The Graphics migration was a real compatibility obstacle, but it needed to be resolved before stacking more renderer architecture on that older base. Otherwise we spend time optimizing costs the newer framework may already have removed, and we lose the benefit of its fixes and qualification.

The packet/rendering redesign also assumed more independence than the renderer actually had. Dropped frames still affected enhanced surfaces; object identity depended on draw order; post-processing still assumed the old full-VRAM surface. Those assumptions explain the flashes, floating trees and ghost HUD. They are specific engineering mistakes we can test and correct, rather than a judgment about anyone's ability.

We also need firmer boundaries around what a test establishes. Stable endpoint replays do not establish equivalence to the old renderer. A zero-unmatched counter does not establish that objects were matched correctly. Sixty presentation completions do not establish sixty distinct, visually correct images on screen. Your notes often acknowledge those limits; the integration and release decision needs to honor them just as carefully.

I recommend rebuilding on current upstream and bringing improvements back in small groups. If an old patch is already covered, keep the upstream version. If it conflicts, revisit its intended behavior instead of preserving its machinery. If an enhancement cannot yet meet visual and runtime acceptance, keep it on a topic branch while the reliable base continues to move forward.

For future AI-assisted work, give each task one hypothesis, a bounded change and a result that can reject it. Ask for the smallest before/after comparison before authorizing another architectural layer. Require the agent to state what remains unverified and to preserve failures through handoffs. After two unsuccessful experiments, stop adding machinery and reassess the approach. Frequent upstream integration and a brief human check at meaningful boundaries would have caught this drift much earlier.

The aim is to preserve your ambition while reducing the amount of custom engine code that has to be understood, maintained and proven correct.
