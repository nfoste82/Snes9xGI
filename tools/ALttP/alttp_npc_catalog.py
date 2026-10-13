"""Bounded ROM draw-state models; names distinguish actual visual families.

Room filters follow SpritePrep_Adults/Storyteller/Shopkeeper in sprite_main.c.
Each model uses a draw routine directly, never the sprite's gameplay dispatcher.
"""


def model(key, name, type_id, draw="default", directions=1, steps=2, **kwargs):
    result = dict(key=key, name=name, type_id=type_id, draw=draw,
                directions=directions, steps=steps, variant=0, follower=0,
                action="modeled draw frames")
    result.update(kwargs)
    return result


CATALOG = [
    model("link", "Link", 0, "link", 4, 6, world=0,
          explicit_contexts=[(1, 0, 0, "Player draw adapter: green tunic, walking, no equipment")],
          action="walking"),
    model("elder", "Sahasrahla / Aginah", 0x16),
    model("smithy", "Dwarven Swordsmith", 0x1a, "smithy", 2, 5, world=0),
    model("frog", "Cursed Swordsmith (Frog)", 0x1a, "frog", 1, 1, world=1),
    model("kid", "Bug-Catching Kid", 0x1f, "kid", 1, 3),
    model("father", "Flute Boy's Father", 0x29, "father", 1, 3, rooms=[0x103]),
    model("hideout", "Man in Thieves' Den", 0x29, "hideout", 4, 2, rooms=[0x119]),
    model("thief-hideout", "Thief Hideout Resident", 0x29, "thief", 4, 3, rooms=[0xe1]),
    model("sweep", "Sweeping Lady", 0x2a, "sweep"),
    model("camper", "Camper", 0x2b, "hobo", 1, 3),
    model("lumber", "Twin Lumberjacks", 0x2c, "lumber", 1, 3),
    model("flute", "Flute Boy (Human)", 0x2e, "flute", 2, 2, world=0),
    model("stump", "Flute Boy (Dark World)", 0x2e, "stump", 1, 4, world=1),
    model("maze-lady", "15-Second Game Operator (Lady)", 0x2f, "lady", 4, 2),
    model("maze-man", "15-Second Game Operator (Man)", 0x30, "maze", 4, 2),
    model("fortune-light", "Fortune Teller (Light World)", 0x31, "fortune", world=0),
    model("fortune-dark", "Fortune Teller (Dark World)", 0x31, "fortune", world=1),
    model("brothers", "Quarreling Brothers", 0x32, directions=4),
    model("snitch-young", "Alarmed Villager (Young)", 0x34, directions=4),
    model("inn", "Kakariko Storyteller", 0x35, steps=1),
    model("witch", "Syrup", 0x36, "witch", 1, 8),
    model("middle", "Average Middle-Aged Man", 0x39, "middle", 1, 1),
    model("bat", "Mad Batter", 0x3a, "single"),
    model("grandson", "Sahasrahla's Grandson", 0x3c, directions=4, steps=1),
    model("snitch-old", "Alarmed Villager (Older) / Human Cucco", 0x3d, "lady", 4, 2),
    model("soldier", "Soldier (Tutorial)", 0x3f, "tutorial", 4, 1),
    model("blue", "Blue Guard", 0x41, directions=4, steps=4),
    model("green", "Green Guard", 0x42, directions=4, steps=4),
    model("red", "Red Guard", 0x43, directions=4, steps=4),
    model("fast", "Fast Spear Guard", 0x44, "spear", 4, 2),
    model("spear", "Spear Guard", 0x45, "spear", 4, 2),
    model("archer", "Blue Archer", 0x46, "archer", 4, 4),
    model("bush-archer", "Green Bush Archer", 0x47, "archer", 4, 4),
    model("javelin", "Red Javelin Guard", 0x48, "javelin", 4, 2),
    model("bush-javelin", "Red Bush Javelin Guard", 0x49, "javelin", 4, 2),
    model("bombguard", "Bomb Guard", 0x4a, "bombguard", 4, 2),
    model("recruit", "Green Knife Guard", 0x4b, "recruit", 4, 2),
    model("zora", "King Zora", 0x52, "zora", 1, 4),
    model("archery", "Archery Game Operator", 0x65, "archery", 1, 5),
    model("queen-light", "Waterfall of Wishing Fairy", 0x72, "queen", world=0),
    model("queen-dark", "Cursed Fairy", 0x72, "queen", world=1),
    model("uncle", "Link's Uncle", 0x73, "uncle", 4, 2, exclude_rooms=[0x12]),
    model("sage", "Loyal Sage", 0x73, "priest", 4, 2, rooms=[0x12]),
    model("running", "Running Man", 0x74, directions=4),
    model("merchant", "Street Merchant", 0x75),
    model("zelda", "Princess Zelda", 0x76, "maiden", 4, 2, follower=1),
    model("wife", "Sahasrahla's Wife", 0x78),
    model("agahnim", "Agahnim", 0x7a, "agahnim", 1, 12),
    model("maiden", "Crystal Maidens (Shared Artwork)", 0xab, "maiden", 4, 2, follower=6,
          context_type=0xb7, context_note="Dynamic crystal-cutscene actor; shared maiden graphics loaded explicitly, palette provisional."),
    model("oldman", "Lost Old Man", 0xad, "oldman", 4, 2, follower=4),
    model("bombshop", "Bomb Shopkeeper", 0xb5, "bombshop"),
    model("kiki", "Kiki", 0xb6, "kiki", 4, 2, follower=10),
    model("blind-maiden", "Blind's Disguise (Maiden)", 0xb7, "maiden", 4, 2, follower=6),
    model("bully", "Bully", 0xb9, "bully", 2, 2),
    model("pinkball", "Bully's Friend", 0xb9, "single"),
    model("shop-light", "Hooded Shopkeeper", 0xbb, "shop", world=0, rooms=[0xff,0x100,0x118]),
    model("shop-dark", "Dark World Shopkeeper", 0xbb, "single", world=1, rooms=[0x10f,0x110,0x11f]),
    model("chestgame", "Game of Chance Operator", 0xbb, "single", rooms=[0x106,0x112]),
    model("nice-thief", "Thief (Friendly)", 0xbb, "thief", 4, 3, rooms=[0x11e,0x123,0x127,0x124,0x125]),
    model("drink", "Kakariko Inn Patron", 0xbc, "drink"),
    model("catfish", "Catfish", 0xc0, "catfish", 1, 7),
    model("thief", "Thief", 0xc4, "thief", 4, 3),
    model("fairy", "Great Fairy", 0xc8, "fairy", 1, 4),
    model("dig", "Digging Game Operator", 0xd5, "dig", 1, 3),
    model("ganon", "Ganon", 0xd6, "ganon", 2, 17,
          action="modeled body draw frames"),
    model("assistant", "Syrup's Assistant", 0xe9, "shop"),
]

for variant, name in enumerate(["Storytelling Bird", "Storytelling Tree", "Storytelling Hand",
                              "Storytelling Octopus", "Storytelling Insect"]):
    entry = model(f"story-{variant}", name, 0x28, "story", rooms=[[0x10e], [0x10e], [0x112], [0x11a], [0x114]][variant])
    entry["variant"] = variant
    CATALOG.append(entry)

MODELED_TYPES = {entry["type_id"] for entry in CATALOG}
