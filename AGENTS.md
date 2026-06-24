# Project Rules For Goldbach Work

1. Always use the `/firedb-kordata` skill when connecting to the FV Database;
   for bigger queries use up to 18 cores.

2. Prior-art comes first. Before treating any mathematical or computational
   idea as new, search the literature and project pages for earlier work. Use
   primary sources where possible: papers, author project pages, official
   repositories, or well-cited survey material.

3. Do not use words such as "discovery", "new method", "breakthrough",
   "proof", or "solution" until prior art has been checked and the claim has
   been compared against known methods. Until then, call it a hypothesis,
   experiment, implementation, replication, diagnostic, or engineering result.

4. If prior art exists, say so plainly and reposition the work honestly:
   implementation, reproduction, measurement, optimization, visualization, or
   negative result. Do not rebrand rediscovered known work as novelty.

5. Every research branch must keep a short note with:
   - the claim being tested;
   - sources checked before computation;
   - what would count as a genuinely new contribution;
   - what was actually found;
   - why the result is or is not publishable.

6. Run computations only after the question is framed against prior work. Do
   not spend CPU on an idea just because it feels elegant or promising.

7. For Goldbach-specific work, distinguish clearly between:
   - finding any decomposition;
   - finding the minimal witness `p(N)`;
   - verifying a finite range under probable-prime tests;
   - producing formal primality certificates;
   - proving a theorem.

8. When in doubt, be more conservative. It is better to publish a precise
   computational note than to overclaim a rediscovery.
