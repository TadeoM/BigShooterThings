# BigShooterThings
34 of them to be specific.


# Controls
- Y: Rewind Time
- E: Teleport
- WASD to move
- left click to shoot
- right click to aim
- Sprint to sprint
- Space to jump

# OnDeath -> Ammo Drop
When a player dies, they drop as many ammo magazines as they currently have left in their current weapon.
This was the first mecahnic I implemented, it allowed me to learn some of the basics of the Engine without much concern for Client Replication(CR) and Server Reconciliation(SR), as just spawning an ammo crate puts it in a check with the server that's automatically handled.

# Teleport Mechanic
When you teleport, you are transported about 100 Units forward.
I implemented this mechanic second. The teleport was interesting because I had to understand how the Shooter Character movement component implmented CR and SR. 
The Saved moves class was really the best help for this, and solved this problem. In the future, I learned the Movement Mods and Flags from the rewind mechanic, and I'd definitely use those next time to implement most mechanics. The teleport didn't require multiframe animations, so it made it easier to implement without the flags.

# Rewind Mechanic
The last mechanic, this Rewind will transport the player to the position which they were in 3 seconds prior, as well as restoring their Health to what it was, similar to tracer.
I knew I was going to need to store the previous positions and a timestamp for each position. (I later also added another set of arrays that kept track of the player's health) This is so that I could lerp/traverse through the positions rather than just teleporting the player back to their previous position. Figuring out how the Custom Flags was probably the most challenging part of this mechanic, but once I learned how they worked it was pretty simple.
For the tracking of poisitons I wanted to implement a Deque, so that I could pop from both the top and bottom of the data structure, however, I learned that Unreal only had a Queue built-in to UE4.27. Because of this, I ended up using an array and removing from the top and bottom when necessary.
