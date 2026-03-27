For this task ONLY look inside Source/Render and Source/Framework and Source/IntegrationTests and Shaders/. DO NOT look elsewhere without asking.

When I have SSR enabled (which requires TAA and ddgi_half_res), and resize the screen _slowly_ then it shows lots of nans/infs, im assuming from motion vectors. can you look into this?

When I just resize the screen from 1000 -> 1500, it doesnt do this. but if im _dragging_ the editor viewport window then this happens.
