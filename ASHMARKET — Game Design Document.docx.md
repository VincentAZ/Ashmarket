# **ASHMARKET — Game Design Document**

Nov 10, 2026 · @Vincent Serpico

## **1\. Overview**

Ashmarket is a single-player, third-person open-world action RPG set in a slave-trading market city, where the player starts as property and can end as liberator, warlord or head slaver.

**Project assumptions**

* No rating board, storefront or console rules apply.  
* Content level is Adults Only: explicit violence, explicit sex, hard drugs, player-driven slavery.  
* No child characters exist anywhere in the world. Every NPC is an adult.  
* Scope is a design exercise first. Section 11 covers whether it becomes a prototype.

**Design pillars**

1. **Everything has a price.** Water, people, safety and loyalty are all bought and sold, and the player always sees the number.  
2. **The town remembers.** Kills, sales, betrayals and mercies change prices, access and who hunts you.  
3. **No clean hands.** Every path to power costs somebody. The game never forces a moral, but it always shows the bill.  
4. **Scrap everything.** Every building, weapon and outfit is visibly built from the old world's wreckage.

**Core loop**

1. Go out: scavenge ruins, raid convoys, hunt bounties, capture or free people.  
2. Come back: sell at the kiosks and the Block, repair, heal, re-arm.  
3. Spend: gear, water, vice, bribes, fighters, property.  
4. Climb: cage rank, faction standing, businesses owned.  
5. The town reacts, and the next trip out is more dangerous.

## **2\. World and factions**

Ashmarket sits on the floor of a drained reservoir in the desert Southwest, about forty years after the collapse, and it survives because it controls the only deep well for 200 miles.

**The collapse.** Never fully explained. Old-timers disagree about what happened, and the game does not settle it. What matters is that the water went first.

**The city.** Roughly 3,000 people live in shipping containers, gutted buses and tarp shacks under a broken freeway overpass. The dam wall is the city wall. One gate in, one gate out.

**The wasteland.** Three zones ring the city: the Flats (salt pan, convoy routes), the Bones (a dead suburb, best scavenging) and the Kiln (open desert, raider camps, lethal by day).

| Faction | Controls | Wants | Leader |
| :---- | :---- | :---- | :---- |
| Chainmen | The Block, the Gut, the collar trade | Monopoly on labor and blood sport | Mother Ledger, a former accountant who keeps every debt in one book |
| Wardens | The gate, the wall, the gallows | Order, and their cut of everything | Captain Voss, paid by the Pump Boss, loyal to whoever pays more |
| The Pump | The well and the water price | To stay indispensable | The Pump Boss, never seen in public, speaks through a pipe |
| Free Tags | Tunnels under the dam, safe houses | To break the collar trade | Nobody knows; cells only know their own members |
| Scrappers | Salvage rights in the Bones | Fair prices and to be left alone | Guildmaster Okafor-Reyes, elected yearly |

**Faction standing** runs from \-100 to \+100 per faction. It sets prices, opens quests and decides who attacks on sight. Chainmen and Free Tags are opposed: gaining with one costs standing with the other.

## **3\. Market Law**

Five laws are painted on the dam wall, and they are both the town's rules and the game's rules.

1. **No killing inside the walls, except in the Gut.** Break it and the Wardens come.  
2. **Every debt gets paid.** In water, in work or in the cage.  
3. **No tag means you're property.** Anyone untagged can be collared by whoever catches them.  
4. **The cage's verdict is final.** Walk out alive and the debt is cleared.  
5. **The Pump sets the price.** Touch the water supply and you hang.

**Heat** is the wanted system, 0 to 5\.

| Heat | Trigger examples | Response |
| :---- | :---- | :---- |
| 1 | Theft, brawling, drawn weapon | Wardens watch you; fine on contact |
| 2 | Assault, robbery of a kiosk | Wardens arrest; pay a fine or lose a weapon |
| 3 | Killing inside the walls | Shoot on sight; kiosks close to you |
| 4 | Killing a Warden or vendor | Bounty posted; hunters follow you into the wasteland |
| 5 | Sabotaging the Pump, mass killing | Gate sealed; every faction hostile until you pay, hide or win |

**Clearing Heat:** pay the fine, bribe Captain Voss, lie low outside the walls, or volunteer for a cage fight under Law 4\.

**Getting caught:** the player is not sent to a loading screen. They are stripped of their tag and put on the Block, and must escape, be bought, or fight out through the Gut again.

**Hanging:** Wardens hang captured NPC lawbreakers at the gate. Bodies stay up for three in-game days.

## **4\. Locations**

Nine locations cover every required feature, and each one has a vendor, a system and at least one quest line.

| Location | Built from | Run by | What the player does there |
| :---- | :---- | :---- | :---- |
| The Last Call (bar) | Fuselage of a crashed airliner | Bartender "Captain" Dolores, who claims she was the pilot | Jobs, rumors, gambling, drinking contests, knife games |
| Rotgut Row (smaller bars) | Oil drums, tarps, a school bus | Rotating owners | Cheap drink, bar fights, drug deals, fences |
| Hide & Plate (clothes and armor) | A container with a sewing-machine line | Twin tailors Needle and Thread | Buy, repair and craft armor; disguises |
| Tooth & Trigger (weapons) | An armored bank truck | Gunsmith Brass, missing three fingers | Buy, mod and repair weapons; ammo; contract kills |
| The Pump (food and water) | The old dam pump house plus a row of grill carts | The Pump Boss's clerks | Buy water and food; water is also the currency |
| Doc Stitch (medical) | An ice-cream truck that still plays the jingle | Doc Stitch, a veterinarian | Heal, cure, buy stims, sell organs, buy drugs |
| The Block (auction) | A flatbed trailer under floodlights | Mother Ledger's auctioneers | Sell captives, bid, shill, rob the sale |
| The Gut (cage arena) | An emptied municipal pool with a welded cage lid | Pit boss Hammond "Ham" Rask | Fight, bet, manage a stable |
| The Velvet Tin (brothel) | Three linked Airstream trailers | Madam Corazon | Client services, intel, blackmail, ownership |

**Kiosk rules shared by all vendors**

* Stock is limited and restocks every three days from convoys. Raid a convoy and that kiosk runs short.  
* Prices move with faction standing, Heat and scarcity.  
* Vendors can be robbed, extorted, bought out or killed. A dead vendor is replaced after a week by a worse one with higher prices.  
* Each vendor has a three-part quest line that ends with the player as partner, owner or enemy.

## **5\. Core systems**

Seven systems drive the game, and water ties them together because it is both the survival resource and the money.

**Water economy**

* Currency is the liter, carried as sealed caps (1 L), jugs (10 L) and drums (100 L).  
* The player drinks about 3 L a day. Drinking your money is the constant pressure.  
* The Pump posts a new price every morning. Raids, sabotage and drought events move it.  
* Dirty water is free in the wasteland and causes infection unless boiled or filtered.

**Combat**

* Third-person, cover-light, melee-heavy. Ammo is scarce, so guns end fights and blades start them.  
* Limb damage on both sides: crippled arms drop weapons, crippled legs slow movement.  
* Dismemberment and finishers on staggered enemies.  
* Downed enemies can be killed, looted, left or collared (section 6).

**Weapons (Tooth & Trigger)**

| Tier | Examples | Trait |
| :---- | :---- | :---- |
| Scrap | Rebar club, sign-post axe, pipe pistol | Cheap; breaks or jams often |
| Tuned | Leaf-spring machete, bolt rifle, sawn shotgun | Reliable; two mod slots |
| Relic | Pre-collapse firearms | Rare; ammo costs more than food |

Weapons wear with use. Brass repairs them for water or for parts.

**Armor and clothes (Hide & Plate)**

* Five slots: head, torso, arms, legs, coat.  
* Materials are tire rubber (cheap, blunt resistance), road sign (bladed resistance) and car door (ballistic resistance, heavy).  
* Clothing sets work as disguises: Warden coat, Chainman leathers, Scrapper rig. Disguises fail at close range and at high Heat.

**Survival and medicine (the Pump, Doc Stitch)**

* Meters: thirst, hunger, infection, addiction.  
* Food spoils in three to six days. Grill carts sell cooked meat of unstated origin; cheap meat carries a sickness roll and some of it is human.  
* Doc Stitch sells bandages, splints, antibiotics and stims, sets limbs, and buys organs. Selling a kidney pays well and permanently cuts stamina by 15%.

**Drugs and vice**

| Substance | Effect | Cost |
| :---- | :---- | :---- |
| Rotgut | Pain resistance up, aim down | Hangover debuff |
| Dust | Speed and melee damage up | Fast addiction, heart damage on overdose |
| Hush | Removes pain and fear effects | Withdrawal halves max health |
| Stims | Instant heal | Each dose heals less for 24 hours |

* The player can deal, cut product to stretch it, or poison a rival's supply.  
* Addiction and withdrawal apply to the player, to fighters in the stable and to NPCs.

**Reputation**

Three tracks, all visible: **Fear** (violence witnessed), **Trust** (deals honored) and **Name** (cage fame). Vendors price on Trust, crowds react to Name, and weak enemies flee from Fear.

## **6\. Slavery system**

Slavery in Ashmarket is based on tags and debt, never on ancestry, and the player can take part in it, fight it, or do both.

**How someone becomes property**

* Captured without a tag outside the walls.  
* Defaulted on a debt recorded in Mother Ledger's book.  
* Sentenced by the Wardens.  
* Sold by a rival, a family member or a faction.

**Diversity rules**

* Captives are generated from the same pool as every other NPC: random ancestry, sex, build, age (all adults) and background.  
* Slavers, buyers, Wardens and Free Tags are drawn from that same pool. No group in the game maps to owner or owned.  
* Mother Ledger's ledger lists debts, skills and tag numbers. It has no field for ancestry, and no NPC dialogue ties worth to it.  
* Visual language is post-apocalyptic: welded collars, stamped dog tags, debt slips, floodlit truck beds. No imagery, costume or vocabulary taken from any real historical slave trade.  
* Backgrounds vary: ex-Warden, bankrupt merchant, raider, doctor, gambler, drifter.

**Capture**

1. Down a target without killing them (blunt weapons, leg shots, Hush darts).  
2. Fit a collar. Collars cost water and can be picked or cut.  
3. Move them to town on foot or by cart. Captives slow the player, call for help and try to run.

Tagged citizens can be captured too, but that is kidnapping under Market Law and raises Heat unless the tag is destroyed first.

**The Block**

* Auctions run every third evening. The player can sell, bid, or pay a shill to push prices.  
* Slaves can be bought or sold by and to brothels   
* Price comes from health, age, skills (medic, mechanic, fighter, cook) and reputation.  
* The Chainmen take 20%. Selling outside the Block is cheaper and illegal.  
* The auction can be robbed, sabotaged or raided with the Free Tags.

**Ownership**

| Assignment | Player gets | Risk |
| :---- | :---- | :---- |
| Scrap crew | Passive salvage income | Theft, slow work, escape |
| Cage stable | Fighters for the Gut (section 7\) | Death, throwing fights |
| Business staff | Runs a kiosk or bar the player owns | Skimming, informing to Wardens |
| Sell on | Water up front | None, and Trust drops with the Free Tags |

Each owned person has **Loyalty** and **Resentment**. Food, medicine and promises raise Loyalty. Beatings, sale threats and broken promises raise Resentment. High Resentment leads to sabotage, escape, or a knife while the player sleeps. Above a threshold across several people, they revolt together.

**Freeing people**

* Buy a tag at the Pump office and hand it over. Freed NPCs may leave, stay as paid crew, or join the Free Tags.  
* Smuggle captives through the dam tunnels with the Free Tags.  
* Burn the ledger. This is a late-game heist that voids every debt in town at once.

**Consequences**

* Slaver players are hunted by Free Tags cells and get worse prices from the Scrappers.  
* Liberator players are hunted by Chainmen collectors and banned from the Gut's betting windows.  
* NPCs the player sold can reappear later: in the cage, behind a kiosk, or holding a gun.

**The player as property**

The game opens with the player collared and sold. The brand given in the intro stays on the character model for the whole game, and any arrest at Heat 3 or higher puts the player back on the Block.

## **7\. The Gut**

Every fight in the Gut ends with one fighter dead, and the player can enter as fighter, owner or gambler.

**Fight rules**

* One on one by default. Special cards run two on two, one against three, or fighter against animal.  
* Weapons are drawn from a rack before the bell: blades, clubs, chains, a shield, nothing ranged.  
* The cage has hazards the pit boss switches on in later rounds: spiked walls, a live generator cable, a flooded floor, released dogs.  
* No surrender. A downed fighter is finished in the ring.  
* The crowd shouts for a finisher style. Matching it pays a bonus and raises Name.

**Ladder**

| Rank | Fights to clear | Reward |
| :---- | :---- | :---- |
| Meat | 3 | The player's own tag (the intro arc) |
| Dog | 5 | Sponsor offers, armor in the ring |
| Butcher | 5 | A cut of the gate |
| Champion | 1, against the standing champion | Ownership stake in the Gut |

**Fighting as the player**

* Injuries carry over between fights and into the open world.  
* Dust and Hush are legal in the ring and have the usual costs.  
* Sponsors pay for wins and make demands: throw a fight, kill slowly, wear their colors.

**Running a stable**

* Buy fighters at the Block or sign free volunteers for a wage.  
* Train three stats: strength, speed, nerve. Training takes days and food.  
* Dope them for a short-term edge and a long-term addiction.  
* Choose the match-ups. A fighter who keeps winning can be promised a tag; breaking the promise spikes Resentment across the whole stable.

**Betting and fixing**

* Odds are posted an hour before each fight and move with the money.  
* The player can bet on any fight, including against their own fighter.  
* Fixes: bribe a fighter, drug the water bucket, tamper with the weapon rack. Each has a detection chance, and a caught fixer goes into the cage.

**After the bell**

Winners are paid in water. The dead are sold to Doc Stitch for parts or to the grill carts.

## **8\. The Velvet Tin**

The Velvet Tin is the town's brothel and its best source of secrets, and the player can use it as a client, a spy or an owner.

**Staff**

* Around a dozen named workers, all adults, mixed in sex, build and background, each with a personality, a price and a quest hook.  
* Madam Corazon runs the house, pays the Wardens for protection and keeps a file on every regular.

**As a client**

* Explicit scenes with nudity, shown on screen, one per worker with variations as the relationship level rises.  
* Effects: a morale buff for a day, a disease roll (treated at Doc Stitch), and a chance of being robbed or photographed if the player is drunk or carrying a lot of water.  
* Repeat visits build a relationship track with that worker: discounts, gossip, a safe bed at high Heat, and eventually a personal quest.

**As a spy**

* Every faction leader has a favorite at the Tin. Their favorites know schedules, debts and weaknesses.  
* Intel is bought with water, favors or relationship level.  
* Blackmail: plant a camera, collect photos, sell them or use them. Madam Corazon retaliates if she catches the player working her rooms without paying her.

**As an owner**

* Madam Corazon can be bought out, partnered with or removed.  
* Management screen: wages, prices, security, Warden bribes, upkeep.  
* Good wages and security raise income and intel quality. Squeezing the staff makes workers quit to a rival house on Rotgut Row and take the regulars with them.

**Quest line: the house debt**

Several workers and most of the cleaning and bar staff owe Mother Ledger, and she wants the house itself as payment. The player can pay the debt, help Corazon fight it, burn the ledger, or hand the Tin to the Chainmen for a cut. The last option ends the house as a free business, turns every worker hostile, and is written as the bleakest outcome in the game.

## **9\. Story**

The main story runs three acts, from the player's sale on the Block to a fight over who owns Ashmarket.

**Act 1: Meat**

* The player is caught in the Kiln, collared, branded and sold to the Gut for 40 liters.  
* Three cage fights earn a tag. The third opponent is the cellmate who kept the player alive.  
* The act ends at the gate with a tag, no water and a debt to Ham Rask for "room and board".

**Act 2: The climb**

* Open structure. The player works for any faction in any order.  
* Each faction has a five-mission arc that ends with an offer: Chainmen (a seat at the Block), Wardens (a badge), Free Tags (a cell of their own), Scrappers (a salvage claim), the Pump (a meeting with the Boss).  
* Mid-point event: a drought doubles the water price, the Block fills with defaulted debtors, and every faction starts moving against the others.

**Act 3: The ledger**

Mother Ledger's book is the real power in town because it records who owes what to whom. Every ending is decided by what happens to it.

| Ending | How | Result |
| :---- | :---- | :---- |
| Burn it | Heist with the Free Tags | All debts void, the collar trade collapses, the town riots and may not survive |
| Take it | Kill or depose Mother Ledger | The player runs the Chainmen and owns half the town |
| Sell it | Hand it to the Pump Boss or Captain Voss | One faction rules outright; the player leaves rich |
| Walk away | Leave through the gate at any point after Act 1 | Ashmarket carries on without the player; the epilogue shows what their choices left behind |

**Recurring characters**

* **Sable**, the cellmate from Act 1\. If the player spares them in the third fight by losing on purpose, both survive at a heavy cost, and Sable returns as ally or rival.  
* **Ham Rask**, who treats the player as an investment for the whole game.  
* **Mother Ledger**, who is polite, exact and never raises her voice.  
* **The Pipe**, the voice of the Pump Boss, whose identity is the game's one mystery.

## **10\. Content rules**

The game is Adults Only with two fixed limits: no child characters and no racial mapping of slavery.

**In the game**

* Killing, dismemberment, executions, public hangings, interrogation with violence.  
* Player-driven capture, sale and ownership of adult NPCs.  
* Death matches, fight fixing, corpse and organ trade, cannibalism.  
* Nudity and explicit sex between adults.  
* Hard drug use, dealing, addiction and overdose, with fictional substances and no real-world recipes.  
* Profanity and cruelty in dialogue without a filter.

**Fixed limits**

1. **No child characters.** Every NPC is an adult, and no dialogue, document or backstory puts children in scenes of sex or violence.  
2. **No racial mapping of slavery.** Section 6's diversity rules apply to every generated and written character.

**Tone guide**

* Cruelty is shown plainly and has a cost somewhere in the systems. Nothing is played for a joke at a victim's expense.  
* Humor is gallows humor from people living in it: vendor patter, betting-window banter, the ice-cream jingle.  
* Victims have names, backgrounds and reactions. The game is darker when the player knows who they sold.

## **11\. Decisions and next steps**

All four choices are settled: third-person, survival meters with a toggle, a playable prototype of one system, built in Unreal.

| Decision | Chosen |
| :---- | :---- |
| Camera | Third-person |
| Survival meters | On, with a difficulty toggle |
| Build or paper | Playable prototype of one system |
| Engine | Unreal Engine 5 |

**Prototype: the Gut**

The Gut is the proposed first system because it needs one small level, one enemy type and no open world, and it tests the combat the whole game depends on.

| Milestone | Contents | Done when |
| :---- | :---- | :---- |
| 1\. Grey-box fight | Third-person template, one cage room, light and heavy melee, block, dodge, stamina | The player can kill one dummy enemy and be killed by it |
| 2\. Opponent AI | One fighter with approach, attack, block and retreat behaviors | A fight lasts 60 to 90 seconds and can go either way |
| 3\. Limbs and finishers | Per-limb damage, stagger state, two finisher animations | A crippled arm drops the weapon; a staggered enemy can be finished |
| 4\. Weapon rack | Four weapons with different reach, speed and damage | Weapon choice visibly changes how a fight plays |
| 5\. Hazards | Spiked wall and live cable, switched on by a round timer | Either fighter can be killed by the cage |
| 6\. Ladder and betting | Three fights in a row, carried injuries, odds and a water payout | The Act 1 "Meat" arc is playable start to finish |

**Unreal notes**

* Start from the built-in third-person template and stay in Blueprints until something is too slow or too tangled.  
* Use free marketplace or Mixamo-style placeholder characters and animations. No custom art until milestone 6 works.  
* Gore, dismemberment and crowd come after milestone 6\. They are presentation, and the fight has to be fun as grey boxes first.  
* Survival meters and the difficulty toggle are outside this prototype; only health, stamina and limb state are needed in the cage.

**Next steps**

* ☒ Confirm the four decisions  
* ☒ Confirm the Gut as the prototype system, or pick another  
* ☐ Install Unreal Engine 5 and create the project from the third-person template  
* ☐ Build milestones 1 to 6 in order  
* ☐ In parallel on paper: map of Ashmarket, numbers pass, faction quest lists, character sheets