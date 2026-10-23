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
    model("link", "Link", 0, "link", 4, 9, world=0,
          explicit_contexts=[(1, 0, 0, "Player draw adapter: ROM-backed body and equipment actions")],
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

# Bounded draw-table states, not gameplay dispatchers. Dungeon contexts retain
# their actual room-header palette selectors, independently of graphics sets.
for key, name, type_id, draw, directions, steps in [
    ("keese", "Keese (Bat)", 0x6f, "large", 1, 3),
    ("rat", "Rat", 0x6d, "rat", 4, 4),
    ("rope", "Rope (Snake)", 0x6e, "rope", 4, 2),
    ("armos-knight", "Armos Knights", 0x53, "armos-knight", 1, 4),
    ("armos-statue", "Armos Statue", 0x51, "armos-statue", 1, 1),
    ("hardhat", "Hardhat Beetle", 0x26, "hardhat", 1, 2),
    ("mini-helmasaur", "Mini Helmasaur", 0x13, "mini-helmasaur", 4, 2),
    ("stalfos", "Stalfos", 0xa7, "stalfos", 4, 2),
    ("gibdo", "Gibdo", 0x8b, "gibdo", 4, 2),
    ("wizzrobe", "Wizzrobe", 0x9b, "wizzrobe", 4, 2),
    ("freezor", "Freezor", 0xa1, "freezor", 1, 8),
    ("pengator", "Pengator", 0x99, "pengator", 4, 5),
    ("wallmaster", "Wallmaster", 0x90, "wallmaster", 1, 2),
    ("zol", "Zol (Blob)", 0x8f, "zol", 1, 8),
    ("red-bari", "Red Bari", 0x23, "red-bari", 1, 2),
    ("blue-bari", "Blue Bari", 0x22, "large", 1, 2),
    ("popo", "Popo", 0x4e, "popo", 1, 2),
    ("eyegore-green", "Green Eyegore", 0x83, "eyegore", 1, 12),
    ("eyegore-red", "Red Eyegore", 0x84, "eyegore", 1, 12),
    ("beamos", "Beamos", 0x61, "beamos", 1, 32),
    ("mothula", "Mothula", 0x88, "mothula", 1, 3),
    ("blind-boss", "Blind (Boss)", 0xce, "blind", 1, 15),
    ("arrghus", "Arrghus", 0x8c, "arrghus", 1, 3),
    ("kholdstare", "Kholdstare", 0xa2, "kholdstare", 1, 16),
    ("vitreous", "Vitreous", 0xbd, "vitreous", 1, 6),
    ("lanmola", "Lanmolas", 0x54, "lanmola", 4, 2),
    ("moldorm-boss", "Moldorm (Boss)", 0x09, "moldorm-boss", 4, 4),
    ("helmasaur-king", "Helmasaur King", 0x92, "helmasaur-king", 1, 12),
]:
    CATALOG.append(model(key, name, type_id, draw, directions, steps,
                         dungeon_palette=True, action="modeled body draw frames"))

next(entry for entry in CATALOG if entry["key"] == "wallmaster").update(
    context_type=0x109, context_kind="overlord",
    context_note="Dynamically spawned by room's Wallmaster overlord 09")

for variant, name in enumerate(["Storytelling Bird", "Storytelling Tree", "Storytelling Hand",
                              "Storytelling Octopus", "Storytelling Insect"]):
    entry = model(f"story-{variant}", name, 0x28, "story", rooms=[[0x10e], [0x10e], [0x112], [0x11a], [0x114]][variant])
    entry["variant"] = variant
    CATALOG.append(entry)

MODELED_TYPES = {entry["type_id"] for entry in CATALOG}
