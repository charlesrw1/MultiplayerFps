Aero stances: tops, hoods, drops
Different aero penalty, stamina penalty, handling, brake ability, pack skills

Stamina: different settings. Roughly match watts of person in front. If they coast you coast, but no automatic braking. Must watch wheel overlap too.

Handling: degrades based on stamina, also affected by aero choice. Responsiveness.

No gear selection, automatic. But you _can_ choose watts, coast, and brake on your own.

Important: animations, sounds, camera work.
Animations for conveying stamina, current watts, aero, handling weight, hill grade. Pack riding.
Sounds for all bike effects: gear shifts, road noise, free wheel, wind, braking.
Camera: 3rd person, must convey speed and weight, partially cinematic. Can see riders wheel _in front_. Make player model transparent? Or overhead view? Or partially side offset over shoulder?

Animation:
Gradient (shifting weight, uphill has different center of balance)
Stamina (more exxagerated movements at lower stamina, rocking of shoulders etc)
Effort (harder pedal strokes etc)
Aero/hand position (hoods, top, drops, out of saddle)
transitions (to/from out of saddle, from pedaling to stop pedaling (spring effect))
Cadence (lower cadence affects body movement, more laborded)
Turns (turning of bars, leaning in)
Deceleration (shifting of weight forward)
Leaning into other riders when in pack

Subtle Feedback:
Lower cadence when gradient pitches up, then hear gear click
Higher cadence when gradient pitches down, then hear gear click
Higher wind sfx when not in the draft
NPC barks about slowing etc
Wind representation on clothing? Flapping etc
Rain: rain being picked up by bike tires when behind someone, jerseys getting dirty over race?
Tattered jersey after fall?
When at slow speeds uphill, bike turns on its own, must correct this manually.
More responsive handling at higher stamina.
Brake locking up on downhill
When in really steep hill, bike "lurches" forward on pedal strokes, then decelerates then lurches etc. Not a constant velocity.
Chain slapping chainstay when going over bumps. Chain bouncing animation! Rider being bumped down on bumps, arms further bent, body sent downwards. Bike shaking when going over bumps, rider has a bit of springyness to not be as bounced around.

TV camera motos, helicopters.
End of race broadcast! Show clips from moto and helicopter perspective. Very cool. LLM generated dialogue?
See a PiP broadcast view? With commentary audio?
Bell lap. 
Slightly out of saddle on descents? With "springy" ness on bumps? 

Representing bumps in road as camera shake. Bumpy terrain -> screen shake? or at least some type of blur/jitter efffect along with SFX

Pack riding? 
To try to take someones spot: lean into them, they can "challenge" you and lean back. Leaning too much however for too long or too sudden and you will crash yourself. Leaning can apply pressure to try to push someone out of the way
Find gaps. In more relaxed peleton state, riders wont challenge for the spot and might even coast to let you move in. But in faster, tense situations, AI will challenge every spot. Challenging spot reduces power output

Race jerseys:
have a season rainbow champion jersey. also race jerseys etc
goal: buy houses around the map. spread sponsors buisness. free roam mode? might as well. 

Rider types?
Definetley have customization for skin color, freckles, face shape, hair, eyes, etc.
Also rider height+weight affects look.

Different rider heights+weight?
Low weight -> better hills, but less watts
Smaller -> better aero, but worse watts
Muscular -> better sprint

separate height slider -> affects aero and weight and power output.
then have a 3 way blend of specialization:
     sprint (high peak power)
     climb (low weight)
     tt (good aero and sustained watts)

 
Pedal strikes? Must coast manually in turns? If you take better line, then dont have to coast -> rewards better lines. Start/stop hard efforts drain legs, so minimize that. 

Random fluctuation in rider performance per race, "who has legs" that day. Team is informed who. Also give "bad day" penalty to some riders. Adds randomness to race. Use this to build strategy around. More randomness: weather. (wind, rain) randomness from other teams strategy. per rider randomness in performance. randomness also determined by fatigue over the season or GT. more fatigue -> worse randomness luck. 25% chance for good legs. "bad day" penalty can go up to 50%. But its a normally distributed float. 

Sun affects stamina regen -> no shade, faster stamina drain and worse regen. Water can help stamina regen. 

Water during race? Dont want it to be "drink water to win"...
water helps stamina regen
getting more bottles? fall back to back of peleton? maybe. 

Discreet power levels  150, 200, 250,  275,  300, 325, 350, 400, 450, 500, 600, 800, 1000, MAX
Can also adjust subdivisions, etc.
If following behind someone, then if you match a similar power, then the power automatically adjusts based on speed needed. So if only 280 needed, then being at 300 will adjust down to 280. However, 325 will use 325 watts or 250 will use 250 watts, and thus you will not be matching pace. Game will indicate this. So when power of the peleton changes, you must be changing yours manually too or you will fall behind.

Cadence and gears adjusts to power, this gives some feedback. Rider automatically targets ~90rpm and higher when in sprint power. but it will flucuate 80-100 based on actual conditions. This gives feedback when the rider is speeding up or slowing down based on immedate events like a pitch up in gradient or being out in the wind etc
Power will also try to move to get to 90rpm if it can, so if 250 gets you 70rpm and 275 gets you 100rpm, it might try to target 260W etc.
coasting must be done automatically, but system will warn you when you are in danger and need to coast or down power ASAP if about to crash in under a second at current rate.

power level choosen is a _guide_, not the official power. 

In a stage race, if you fall behind because of crash, then you can spawn back in behind peleton. But there is a stamina penalty based on distance and peleton speed.

crashes: dont want pileups... if you crash then your character goes untouchable and plays a fall animation. then has to catch up or whatever.

system detects when a crash is "your" fault and only punishes you. ie dive bominb corners. swinging out and clipping someones wheel can also be detected and crash you out.

more danger of slide outs in rain or gravel conditions... must watch road for puddles etc.

hard falls might injure you and force you to drop out of race.

Weather effects:
sun, high heat, rain, wind. high heat results in higher stamina drain, along with sun. wind has obvious effects, also implment gusty winds that has slight effect on rider handling when gust hits rider. rain, obvious effects. 

