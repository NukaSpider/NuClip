#include "platform/AppCatalog.h"

#include "core/Paths.h"

#include <QRegularExpression>
#include <QStringList>

namespace nuclip {
namespace {

struct Rule {
    const char *id;
    const char *name;
    const char *exes;
    const char *needles;
};

// needles, when set, must appear in the window title or product name.
// Empty needles match the executable alone.
const Rule kRules[] = {
    {"roblox", "Roblox", "robloxplayerbeta.exe;robloxplayer.exe;robloxapp.exe;roblox.exe", ""},
    {"minecraft", "Minecraft", "minecraft.windows.exe;minecraft.exe", ""},
    {"minecraft-java", "Minecraft", "javaw.exe;java.exe", "minecraft"},
    {"hytale", "Hytale", "hytale.exe;hytaleclient.exe", ""},
    {"vintage-story", "Vintage Story", "vintagestory.exe", ""},

    {"cs2", "Counter-Strike 2", "cs2.exe", ""},
    {"csgo", "Counter-Strike: Global Offensive", "csgo.exe", ""},
    {"valorant", "VALORANT", "valorant-win64-shipping.exe;valorant.exe", ""},
    {"deadlock", "Deadlock", "deadlock.exe", ""},
    {"fortnite", "Fortnite", "fortniteclient-win64-shipping.exe;fortniteclient-win64-shipping_eac.exe;fortnitelauncher.exe", ""},
    {"apex", "Apex Legends", "r5apex.exe;r5apex_dx12.exe", ""},
    {"titanfall2", "Titanfall 2", "titanfall2.exe", ""},
    {"overwatch2", "Overwatch 2", "overwatch.exe", ""},
    {"thefinals", "The Finals", "discovery.exe", ""},
    {"marvelrivals", "Marvel Rivals", "marvel-win64-shipping.exe;marvelrivals-win64-shipping.exe", ""},
    {"deltaforce", "Delta Force", "deltaforceclient-win64-shipping.exe", ""},
    {"arenabreakout", "Arena Breakout", "uagame.exe", ""},
    {"halo-infinite", "Halo Infinite", "haloinfinite.exe", ""},
    {"halo-mcc", "Halo: The Master Chief Collection", "mcc-win64-shipping.exe", ""},
    {"gears5", "Gears 5", "gears5.exe", ""},
    {"splitgate", "Splitgate", "portalwars-win64-shipping.exe", ""},
    {"destiny2", "Destiny 2", "destiny2.exe", ""},
    {"division2", "Tom Clancy's The Division 2", "thedivision2.exe", ""},
    {"warframe", "Warframe", "warframe.x64.exe", ""},
    {"paladins", "Paladins", "paladins.exe", ""},
    {"tf2", "Team Fortress 2", "tf_win64.exe;tf.exe", ""},
    {"l4d2", "Left 4 Dead 2", "left4dead2.exe", ""},
    {"gmod", "Garry's Mod", "gmod.exe", ""},
    {"hl2-tf2", "Team Fortress 2", "hl2.exe", "team fortress"},
    {"hl2-l4d", "Left 4 Dead 2", "hl2.exe", "left 4 dead"},
    {"hl2-gmod", "Garry's Mod", "hl2.exe", "garry"},
    {"brawlhalla", "Brawlhalla", "brawlhalla.exe", ""},
    {"aimlab", "Aimlabs", "aimlab.exe;aimlab_tb.exe", ""},
    {"kovaak", "KovaaK's", "fpsaimtrainer.exe", ""},
    {"osu", "osu!", "osu!.exe", ""},

    {"rocketleague", "Rocket League", "rocketleague.exe", ""},
    {"fallguys", "Fall Guys", "fallguys_client_game.exe;fallguys_client.exe", ""},
    {"amongus", "Among Us", "among us.exe", ""},
    {"goosegooseduck", "Goose Goose Duck", "goose goose duck.exe", ""},
    {"vrchat", "VRChat", "vrchat.exe", ""},
    {"recroom", "Rec Room", "recroom.exe", ""},

    {"gtav-enhanced", "Grand Theft Auto V Enhanced", "gta5_enhanced.exe", ""},
    {"gtav", "Grand Theft Auto V", "gta5.exe;playgtav.exe", ""},
    {"gtaiv", "Grand Theft Auto IV", "gtaiv.exe", ""},
    {"rdr2", "Red Dead Redemption 2", "rdr2.exe", ""},
    {"warzone", "Warzone", "warzone.exe", ""},
    {"warzone-mode", "Warzone", "cod.exe;modernwarfare.exe;blackopscoldwar.exe;cod23-cod.exe;cod24-cod.exe;cod25-cod.exe;cod26-cod.exe;sp22-cod.exe;sp23-cod.exe;vanguard.exe", "warzone"},
    {"cod", "Call of Duty", "cod.exe;modernwarfare.exe;blackopscoldwar.exe;cod23-cod.exe;cod24-cod.exe;cod25-cod.exe;cod26-cod.exe;sp22-cod.exe;sp23-cod.exe;vanguard.exe", ""},

    {"league", "League of Legends", "league of legends.exe", ""},
    {"dota2", "Dota 2", "dota2.exe", ""},
    {"hearthstone", "Hearthstone", "hearthstone.exe", ""},
    {"lor", "Legends of Runeterra", "lor.exe", ""},
    {"mtga", "MTG Arena", "mtga.exe", ""},
    {"masterduel", "Yu-Gi-Oh! Master Duel", "masterduel.exe", ""},
    {"eternalreturn", "Eternal Return", "eternalreturn.exe", ""},

    {"rust", "Rust", "rustclient.exe", ""},
    {"dayz", "DayZ", "dayz_x64.exe;dayz.exe", ""},
    {"pubg", "PUBG", "tslgame.exe;execpubg.exe", ""},
    {"r6", "Rainbow Six Siege", "rainbowsix.exe;rainbowsix_vulkan.exe", ""},
    {"tarkov", "Escape from Tarkov", "escapefromtarkov.exe", ""},
    {"hunt", "Hunt: Showdown", "huntgame.exe", ""},
    {"grayzone", "Gray Zone Warfare", "grayzonewarfare.exe", ""},
    {"darkanddarker", "Dark and Darker", "dungeoncrawler.exe", ""},
    {"bodycam", "Bodycam", "bodycam-win64-shipping.exe", ""},
    {"readyornot", "Ready or Not", "readyornot-win64-shipping.exe", ""},
    {"insurgency", "Insurgency: Sandstorm", "insurgencyclient-win64-shipping.exe", ""},
    {"hellletloose", "Hell Let Loose", "hll-win64-shipping.exe", ""},
    {"squad", "Squad", "squadgame.exe", ""},
    {"postscriptum", "Squad 44", "postscriptum.exe", ""},
    {"arma3", "Arma 3", "arma3_x64.exe;arma3.exe", ""},
    {"arma-reforger", "Arma Reforger", "armareforgersteam.exe;armareforger.exe", ""},
    {"scum", "SCUM", "scum.exe", ""},
    {"7dtd", "7 Days to Die", "7daystodie.exe", ""},
    {"zomboid", "Project Zomboid", "projectzomboid64.exe", ""},
    {"ark-asa", "ARK: Survival Ascended", "arkascended.exe", ""},
    {"ark-ase", "ARK: Survival Evolved", "shootergame.exe", "ark"},
    {"sons-forest", "Sons of the Forest", "sonsoftheforest.exe", ""},
    {"theforest", "The Forest", "theforest.exe", ""},
    {"greenhell", "Green Hell", "greenhell.exe", ""},
    {"thelongdark", "The Long Dark", "thelongdark.exe", ""},
    {"subnautica", "Subnautica", "subnautica.exe", ""},
    {"subnautica-bz", "Subnautica: Below Zero", "subnauticazero.exe", ""},
    {"raft", "Raft", "raft.exe", ""},
    {"valheim", "Valheim", "valheim.exe", ""},
    {"enshrouded", "Enshrouded", "enshrouded.exe", ""},
    {"vrising", "V Rising", "vrising.exe", ""},
    {"palworld", "Palworld", "palworld-win64-shipping.exe", ""},
    {"corekeeper", "Core Keeper", "corekeeper.exe", ""},
    {"grounded", "Grounded", "maine-win64-shipping.exe", ""},
    {"oncehuman", "Once Human", "once_human.exe", ""},
    {"nightingale", "Nightingale", "nightingale.exe", ""},
    {"dune", "Dune: Awakening", "duneawakening.exe", ""},
    {"soulmask", "Soulmask", "ws-win64-shipping.exe", ""},

    {"bf6", "Battlefield 6", "bf6.exe", ""},
    {"bf2042", "Battlefield 2042", "bf2042.exe", ""},
    {"bfv", "Battlefield V", "bfv.exe", ""},
    {"bf1", "Battlefield 1", "bf1.exe", ""},
    {"bf4", "Battlefield 4", "bf4.exe", ""},
    {"payday2", "PAYDAY 2", "payday2_win32_release.exe", ""},
    {"payday3", "PAYDAY 3", "payday3client.exe;payday3client-win64-shipping.exe", ""},
    {"drg", "Deep Rock Galactic", "fsd-win64-shipping.exe", ""},
    {"helldivers2", "Helldivers 2", "helldivers2.exe", ""},
    {"lethalcompany", "Lethal Company", "lethal company.exe", ""},
    {"contentwarning", "Content Warning", "content warning.exe", ""},
    {"repo", "R.E.P.O.", "repo.exe", ""},
    {"schedule1", "Schedule I", "schedule i.exe;schedulei.exe", ""},
    {"peak", "PEAK", "peak.exe", ""},
    {"phasmophobia", "Phasmophobia", "phasmophobia.exe", ""},
    {"devour", "DEVOUR", "devour.exe", ""},
    {"dbd", "Dead by Daylight", "deadbydaylight-win64-shipping.exe", ""},
    {"outlasttrials", "The Outlast Trials", "theoutlasttrials.exe", ""},
    {"backrooms", "Escape the Backrooms", "escapethebackrooms-win64-shipping.exe", ""},

    {"warthunder", "War Thunder", "aces.exe", ""},
    {"enlisted", "Enlisted", "enlisted.exe", ""},
    {"wot", "World of Tanks", "worldoftanks.exe", ""},
    {"wows", "World of Warships", "worldofwarships.exe", ""},
    {"squad44", "Squad 44", "squad44.exe", ""},
    {"chivalry2", "Chivalry 2", "chivalry2-win64-shipping.exe", ""},
    {"mordhau", "Mordhau", "mordhau.exe", ""},
    {"stalker2", "S.T.A.L.K.E.R. 2", "stalker2-win64-shipping.exe", ""},
    {"metroexodus", "Metro Exodus", "metroexodus.exe", ""},
    {"atomicheart", "Atomic Heart", "atomicheart-win64-shipping.exe", ""},

    {"eldenring", "Elden Ring", "eldenring.exe", ""},
    {"nightreign", "Elden Ring Nightreign", "nightreign.exe", ""},
    {"sekiro", "Sekiro", "sekiro.exe", ""},
    {"ds3", "Dark Souls III", "darksoulsiii.exe", ""},
    {"dsr", "Dark Souls Remastered", "darksoulsremastered.exe", ""},
    {"ac6", "Armored Core VI", "armoredcore6.exe", ""},
    {"liesofp", "Lies of P", "lop-win64-shipping.exe", ""},
    {"wukong", "Black Myth: Wukong", "b1-win64-shipping.exe", ""},
    {"stellarblade", "Stellar Blade", "sb-win64-shipping.exe", ""},
    {"expedition33", "Clair Obscur: Expedition 33", "expedition33.exe;sandfall-win64-shipping.exe", ""},
    {"silenthill2", "Silent Hill 2", "shproto-win64-shipping.exe", ""},
    {"re4", "Resident Evil 4", "re4.exe", ""},
    {"re2", "Resident Evil 2", "re2.exe", ""},
    {"re3", "Resident Evil 3", "re3.exe", ""},
    {"revillage", "Resident Evil Village", "re8.exe", ""},
    {"deadspace", "Dead Space", "dead space.exe", ""},
    {"alanwake2", "Alan Wake 2", "alanwake2.exe", ""},
    {"control", "Control", "control_dx11.exe;control_dx12.exe", ""},
    {"doom-eternal", "DOOM Eternal", "doometernalx64vk.exe", ""},
    {"doom-tda", "DOOM: The Dark Ages", "doomthedarkages.exe", ""},
    {"doom2016", "DOOM", "doomx64vk.exe;doomx64.exe", ""},
    {"ultrakill", "ULTRAKILL", "ultrakill.exe", ""},
    {"cyberpunk", "Cyberpunk 2077", "cyberpunk2077.exe", ""},
    {"witcher3", "The Witcher 3", "witcher3.exe", ""},
    {"bg3", "Baldur's Gate 3", "bg3.exe;bg3_dx11.exe", ""},
    {"dos2", "Divinity: Original Sin 2", "eocapp.exe", ""},
    {"disco", "Disco Elysium", "disco.exe", ""},
    {"rogue-trader", "Warhammer 40,000: Rogue Trader", "wh40krt.exe", ""},
    {"wrath", "Pathfinder: Wrath of the Righteous", "wrath.exe", ""},
    {"hogwarts", "Hogwarts Legacy", "hogwartslegacy.exe", ""},
    {"starfield", "Starfield", "starfield.exe", ""},
    {"skyrimse", "The Elder Scrolls V: Skyrim", "skyrimse.exe", ""},
    {"skyrim", "The Elder Scrolls V: Skyrim", "tesv.exe", ""},
    {"fallout4", "Fallout 4", "fallout4.exe", ""},
    {"fallout76", "Fallout 76", "fallout76.exe", ""},
    {"mhw", "Monster Hunter: World", "monsterhunterworld.exe", ""},
    {"mhr", "Monster Hunter Rise", "monsterhunterrise.exe", ""},
    {"mhwilds", "Monster Hunter Wilds", "monsterhunterwilds.exe", ""},
    {"dd2", "Dragon's Dogma 2", "dd2.exe", ""},
    {"gow", "God of War", "gow.exe", ""},
    {"gowr", "God of War Ragnarök", "gowr.exe", ""},
    {"hzd", "Horizon Zero Dawn", "horizonzerodawn.exe", ""},
    {"hfw", "Horizon Forbidden West", "horizonforbiddenwest.exe", ""},
    {"ghost", "Ghost of Tsushima", "ghostoftsushima.exe", ""},
    {"spiderman", "Marvel's Spider-Man", "spider-man.exe", ""},
    {"miles", "Marvel's Spider-Man: Miles Morales", "milesmorales.exe", ""},
    {"tlou", "The Last of Us", "tlou-i.exe", ""},
    {"uncharted", "UNCHARTED: Legacy of Thieves", "u4.exe", ""},
    {"deathstranding", "Death Stranding", "ds.exe", ""},
    {"kcd", "Kingdom Come: Deliverance", "kingdomcome.exe", ""},
    {"kcd2", "Kingdom Come: Deliverance II", "kcd2.exe", ""},
    {"bannerlord", "Mount & Blade II: Bannerlord", "bannerlord.exe", ""},
    {"warband", "Mount & Blade: Warband", "mb_warband.exe", ""},
    {"manorlords", "Manor Lords", "manorlords.exe;manorlords-win64-shipping.exe", ""},
    {"ittakestwo", "It Takes Two", "ittakestwo.exe", ""},
    {"splitfiction", "Split Fiction", "splitfiction.exe", ""},
    {"awayout", "A Way Out", "awayout.exe", ""},
    {"jedifallen", "Star Wars Jedi: Fallen Order", "swgame-win64-shipping.exe", ""},
    {"jedisurvivor", "Star Wars Jedi: Survivor", "jedisurvivor.exe", ""},
    {"outlaws", "Star Wars Outlaws", "starwarsoutlaws.exe", ""},

    {"acshadows", "Assassin's Creed Shadows", "acshadows.exe", ""},
    {"acmirage", "Assassin's Creed Mirage", "acmirage.exe", ""},
    {"acvalhalla", "Assassin's Creed Valhalla", "acvalhalla.exe", ""},
    {"acodyssey", "Assassin's Creed Odyssey", "acodyssey.exe", ""},
    {"farcry6", "Far Cry 6", "farcry6.exe", ""},
    {"farcry5", "Far Cry 5", "farcry5.exe", ""},
    {"watchdogs2", "Watch Dogs 2", "watchdogs2.exe", ""},
    {"watchdogslegion", "Watch Dogs: Legion", "watchdogslegion.exe", ""},
    {"grw", "Ghost Recon Wildlands", "grw.exe", ""},
    {"grb", "Ghost Recon Breakpoint", "grb.exe", ""},
    {"avatar", "Avatar: Frontiers of Pandora", "afop.exe", ""},

    {"sf6", "Street Fighter 6", "streetfighter6.exe", ""},
    {"tekken8", "Tekken 8", "polaris-win64-shipping.exe;tekken 8.exe", ""},
    {"mk1", "Mortal Kombat 1", "mk12.exe", ""},
    {"ggst", "Guilty Gear -Strive-", "ggst-win64-shipping.exe", ""},
    {"dbfz", "Dragon Ball FighterZ", "dbfighterz.exe", ""},
    {"p3r", "Persona 3 Reload", "p3r.exe", ""},
    {"p5r", "Persona 5 Royal", "p5r.exe", ""},
    {"metaphor", "Metaphor: ReFantazio", "metaphor.exe", ""},

    {"poe2", "Path of Exile 2", "pathofexile.exe;pathofexile_x64.exe;pathofexilesteam.exe", "path of exile 2"},
    {"poe", "Path of Exile", "pathofexile.exe;pathofexile_x64.exe;pathofexilesteam.exe", ""},
    {"lastepoch", "Last Epoch", "lastepoch.exe", ""},
    {"diablo4", "Diablo IV", "diablo iv.exe", ""},
    {"diablo2r", "Diablo II: Resurrected", "d2r.exe", ""},
    {"diablo3", "Diablo III", "diablo iii.exe;diablo iii64.exe", ""},
    {"lostark", "Lost Ark", "lostark.exe", ""},
    {"newworld", "New World: Aeternum", "newworld.exe", ""},
    {"throne", "Throne and Liberty", "tl.exe", ""},
    {"bdo", "Black Desert", "blackdesert64.exe", ""},
    {"wow", "World of Warcraft", "wow.exe;wowclassic.exe;wow-64.exe", ""},
    {"ffxiv", "Final Fantasy XIV", "ffxiv_dx11.exe", ""},
    {"gw2", "Guild Wars 2", "gw2-64.exe", ""},
    {"eso", "The Elder Scrolls Online", "eso64.exe", ""},
    {"swtor", "Star Wars: The Old Republic", "swtor.exe", ""},
    {"osrs", "Old School RuneScape", "osclient.exe", ""},
    {"runescape", "RuneScape", "rs2client.exe", ""},
    {"runelite", "RuneLite", "runelite.exe", ""},
    {"albion", "Albion Online", "albion-online.exe", ""},
    {"maplestory", "MapleStory", "maplestory.exe", ""},
    {"genshin", "Genshin Impact", "genshinimpact.exe", ""},
    {"starrail", "Honkai: Star Rail", "starrail.exe", ""},
    {"zzz", "Zenless Zone Zero", "zenlesszonezero.exe", ""},
    {"wuwa", "Wuthering Waves", "wuthering waves.exe", ""},
    {"wuwa-client", "Wuthering Waves", "client-win64-shipping.exe", "wuthering"},
    {"tof", "Tower of Fantasy", "qrsl.exe", ""},
    {"nikke", "GODDESS OF VICTORY: NIKKE", "nikke.exe", ""},
    {"pgr", "Punishing: Gray Raven", "pgr.exe", ""},
    {"naraka", "NARAKA: BLADEPOINT", "narakabladepoint.exe", ""},
    {"sot", "Sea of Thieves", "sotgame.exe", ""},

    {"terraria", "Terraria", "terraria.exe", ""},
    {"starbound", "Starbound", "starbound.exe", ""},
    {"stardew", "Stardew Valley", "stardew valley.exe", ""},
    {"hollowknight", "Hollow Knight", "hollow_knight.exe", ""},
    {"silksong", "Hollow Knight: Silksong", "hollow knight silksong.exe", ""},
    {"hades", "Hades", "hades.exe", ""},
    {"hades2", "Hades II", "hades2.exe", ""},
    {"deadcells", "Dead Cells", "deadcells.exe", ""},
    {"celeste", "Celeste", "celeste.exe", ""},
    {"cuphead", "Cuphead", "cuphead.exe", ""},
    {"hk-silksong-ship", "Hollow Knight: Silksong", "silksong.exe", ""},
    {"davethediver", "Dave the Diver", "davethediver.exe", ""},
    {"balatro", "Balatro", "balatro.exe", ""},
    {"slaythespire", "Slay the Spire", "slaythespire.exe", ""},
    {"slaythespire2", "Slay the Spire 2", "slaythespire2.exe", ""},
    {"vampiresurvivors", "Vampire Survivors", "vampire survivors.exe", ""},
    {"brotato", "Brotato", "brotato.exe", ""},
    {"ror2", "Risk of Rain 2", "risk of rain 2.exe", ""},
    {"cultofthelamb", "Cult of the Lamb", "cult of the lamb.exe", ""},
    {"dst", "Don't Starve Together", "dontstarve_steam64.exe", ""},
    {"rimworld", "RimWorld", "rimworldwin64.exe", ""},
    {"oni", "Oxygen Not Included", "oxygennotincluded.exe", ""},
    {"factorio", "Factorio", "factorio.exe", ""},
    {"satisfactory", "Satisfactory", "factorygame.exe;factorygamesteam.exe;factorygameegs.exe", ""},
    {"dsp", "Dyson Sphere Program", "dspgame.exe", ""},
    {"ksp", "Kerbal Space Program", "ksp_x64.exe", ""},
    {"ksp2", "Kerbal Space Program 2", "ksp2_x64.exe", ""},
    {"ck3", "Crusader Kings III", "ck3.exe", ""},
    {"eu4", "Europa Universalis IV", "eu4.exe", ""},
    {"hoi4", "Hearts of Iron IV", "hoi4.exe", ""},
    {"stellaris", "Stellaris", "stellaris.exe", ""},
    {"vic3", "Victoria 3", "victoria3.exe", ""},
    {"cities1", "Cities: Skylines", "cities.exe", ""},
    {"cities2", "Cities: Skylines II", "cities2.exe", ""},
    {"civ6", "Civilization VI", "civilizationvi.exe;civilizationvi_dx12.exe", ""},
    {"civ7", "Civilization VII", "civilizationvii.exe", ""},
    {"tww3", "Total War: Warhammer III", "warhammer3.exe", ""},
    {"aoe2", "Age of Empires II: Definitive Edition", "aoe2de_s.exe", ""},
    {"aoe4", "Age of Empires IV", "reliccardinal.exe", ""},
    {"coh3", "Company of Heroes 3", "coh3.exe", ""},
    {"sc2", "StarCraft II", "sc2_x64.exe", ""},
    {"wc3", "Warcraft III: Reforged", "warcraft iii.exe", ""},
    {"hots", "Heroes of the Storm", "heroesofthestorm_x64.exe", ""},
    {"fm", "Football Manager", "fm.exe", "football manager"},

    {"forza-h5", "Forza Horizon 5", "forzahorizon5.exe", ""},
    {"forza-h4", "Forza Horizon 4", "forzahorizon4.exe", ""},
    {"forza-m", "Forza Motorsport", "forzamotorsport.exe", ""},
    {"iracing", "iRacing", "iracingsim64dx11.exe", ""},
    {"f125", "F1 25", "f1_25.exe", ""},
    {"f124", "F1 24", "f1_24.exe", ""},
    {"f123", "F1 23", "f1_23.exe", ""},
    {"acc", "Assetto Corsa Competizione", "acc.exe", ""},
    {"ac", "Assetto Corsa", "acs.exe", ""},
    {"acevo", "Assetto Corsa EVO", "assettocorsaevo.exe", ""},
    {"ams2", "Automobilista 2", "ams2.exe", ""},
    {"beamng", "BeamNG.drive", "beamng.drive.x64.exe", ""},
    {"wreckfest", "Wreckfest", "wreckfest.exe", ""},
    {"ets2", "Euro Truck Simulator 2", "eurotrucks2.exe", ""},
    {"ats", "American Truck Simulator", "amtrucks.exe", ""},
    {"snowrunner", "SnowRunner", "snowrunner.exe", ""},
    {"mudrunner", "MudRunner", "mudrunner.exe", ""},
    {"msfs", "Microsoft Flight Simulator", "flightsimulator.exe", ""},
    {"msfs2024", "Microsoft Flight Simulator 2024", "flightsimulator2024.exe", ""},
    {"dcs", "DCS World", "dcs.exe", ""},
    {"xplane", "X-Plane", "x-plane.exe", ""},
    {"nfs-unbound", "Need for Speed Unbound", "needforspeedunbound.exe", ""},
    {"nfs-heat", "Need for Speed Heat", "needforspeedheat.exe", ""},
    {"motorfest", "The Crew Motorfest", "thecrewmotorfest.exe", ""},
    {"trackmania", "Trackmania", "trackmania.exe", ""},
    {"fc26", "EA Sports FC 26", "fc26.exe", ""},
    {"fc25", "EA Sports FC 25", "fc25.exe", ""},
    {"fc24", "EA Sports FC 24", "fc24.exe", ""},
    {"fifa23", "FIFA 23", "fifa23.exe", ""},
    {"madden26", "Madden NFL 26", "madden26.exe", ""},
    {"madden25", "Madden NFL 25", "madden25.exe", ""},
    {"nba2k26", "NBA 2K26", "nba2k26.exe", ""},
    {"nba2k25", "NBA 2K25", "nba2k25.exe", ""},
    {"ufc5", "EA Sports UFC 5", "ufc5.exe", ""},
    {"wwe2k25", "WWE 2K25", "wwe2k25.exe", ""},
    {"fs25", "Farming Simulator 25", "farmingsimulator2025.exe", ""},
    {"fs22", "Farming Simulator 22", "farmingsimulator2022.exe", ""},
    {"powerwash", "PowerWash Simulator", "powerwashsimulator.exe", ""},
    {"houseflipper2", "House Flipper 2", "houseflipper2.exe", ""},
    {"planetzoo", "Planet Zoo", "planetzoo.exe", ""},
    {"planetcoaster2", "Planet Coaster 2", "planetcoaster2.exe", ""},

    {"sims4", "The Sims 4", "ts4_x64.exe", ""},
    {"inzoi", "inZOI", "inzoi.exe;inzoi-win64-shipping.exe", ""},
    {"dreamlight", "Disney Dreamlight Valley", "dreamlightvalley.exe", ""},
    {"palia", "Palia", "palia.exe", ""},
    {"webfishing", "Webfishing", "webfishing.exe", ""},
    {"miside", "MiSide", "misidefull.exe;miside.exe", ""},
    {"buckshot", "Buckshot Roulette", "buckshot roulette.exe", ""},
    {"overcooked2", "Overcooked! 2", "overcooked2.exe", ""},
    {"plateup", "PlateUp!", "plateup.exe", ""},

    {"nms", "No Man's Sky", "nms.exe", ""},
    {"starcitizen", "Star Citizen", "starcitizen.exe", ""},
    {"elite", "Elite Dangerous", "elitedangerous64.exe", ""},
    {"dyinglight2", "Dying Light 2", "dyinglightgame_x64_rwdi.exe", ""},
    {"dyinglight", "Dying Light", "dyinglightgame.exe", ""},

    {"steam", "Steam", "steam.exe", ""},
    {"epic", "Epic Games Launcher", "epicgameslauncher.exe", ""},
    {"riot", "Riot Client", "riotclientservices.exe;riotclientux.exe;riotclientuxrender.exe", ""},
    {"ubisoft", "Ubisoft Connect", "upc.exe;ubisoftconnect.exe", ""},
    {"ea", "EA App", "eadesktop.exe;ealauncher.exe", ""},
    {"origin", "EA App", "origin.exe", ""},
    {"battlenet", "Battle.net", "battle.net.exe", ""},
    {"gog", "GOG Galaxy", "galaxyclient.exe", ""},
    {"xbox", "Xbox", "xboxpcapp.exe", ""},
    {"discord", "Discord", "discord.exe", ""},
    {"spotify", "Spotify", "spotify.exe", ""},
    {"chrome", "Google Chrome", "chrome.exe", ""},
    {"edge", "Microsoft Edge", "msedge.exe", ""},
    {"firefox", "Firefox", "firefox.exe", ""},
    {"brave", "Brave", "brave.exe", ""},
    {"opera", "Opera", "opera.exe", ""},
    {"obs", "OBS Studio", "obs64.exe", ""},
    {"medal", "Medal", "medal.exe", ""},
    {"resolve", "DaVinci Resolve", "resolve.exe", ""},
    {"vscode", "Visual Studio Code", "code.exe", ""},
    {"cursor", "Cursor", "cursor.exe", ""},
    {"devenv", "Visual Studio", "devenv.exe", ""},
};

struct Alias {
    const char *from;
    const char *to;
};

const Alias kAliases[] = {
    {"roblox game client", "Roblox"},
    {"roblox app client", "Roblox"},
    {"robloxplayerbeta", "Roblox"},
    {"valorant-win64-shipping", "VALORANT"},
    {"fortniteclient-win64-shipping", "Fortnite"},
    {"fortniteclient-win64-shipping_eac", "Fortnite"},
    {"r5apex", "Apex Legends"},
    {"r5apex_dx12", "Apex Legends"},
    {"tslgame", "PUBG"},
    {"dayz_x64", "DayZ"},
    {"gta5", "Grand Theft Auto V"},
    {"gta5_enhanced", "Grand Theft Auto V Enhanced"},
    {"cs2", "Counter-Strike 2"},
    {"escapefromtarkov", "Escape from Tarkov"},
    {"cyberpunk2077", "Cyberpunk 2077"},
    {"helldivers2", "Helldivers 2"},
};

const char *kApps[] = {
    "steam",
    "epic",
    "riot",
    "ubisoft",
    "ea",
    "origin",
    "battlenet",
    "gog",
    "xbox",
    "discord",
    "spotify",
    "chrome",
    "edge",
    "firefox",
    "brave",
    "opera",
    "obs",
    "medal",
    "resolve",
    "vscode",
    "cursor",
    "devenv",
};

const char *kLaunchers[] = {
    "steam.exe",
    "steamwebhelper.exe",
    "steamservice.exe",
    "epicgameslauncher.exe",
    "epicwebhelper.exe",
    "riotclientservices.exe",
    "riotclientux.exe",
    "riotclientuxrender.exe",
    "minecraftlauncher.exe",
    "battle.net.exe",
    "leagueclient.exe",
    "leagueclientux.exe",
    "leagueclientuxrender.exe",
    "playgtav.exe",
    "upc.exe",
    "ubisoftconnect.exe",
    "ubisoftgamelauncher.exe",
    "eadesktop.exe",
    "ealauncher.exe",
    "origin.exe",
    "galaxyclient.exe",
    "xboxpcapp.exe",
    "jagexlauncher.exe",
    "overwolf.exe",
    "rockstargameslauncher.exe",
};

const char *kRuntimes[] = {
    "java.exe",
    "javaw.exe",
    "python.exe",
    "pythonw.exe",
    "node.exe",
};

const char *kIgnored[] = {
    "explorer.exe",
    "searchhost.exe",
    "searchapp.exe",
    "shellexperiencehost.exe",
    "startmenuexperiencehost.exe",
    "textinputhost.exe",
    "applicationframehost.exe",
    "systemsettings.exe",
    "dwm.exe",
    "sihost.exe",
    "lockapp.exe",
    "widgetboard.exe",
    "widgets.exe",
    "gamebar.exe",
    "gamebarftserver.exe",
    "runtimebroker.exe",
    "nuclip.exe",
    "pickerhost.exe",
    "shellhost.exe",
};

QString lower(QString text)
{
    return text.trimmed().toLower();
}

bool listed(const char *const *items, int count, const QString &exe)
{
    const QString name = lower(exe);
    for (int i = 0; i < count; ++i) {
        if (name == QLatin1String(items[i]))
            return true;
    }
    return false;
}

bool containsNeedle(const QString &haystack, const QString &needles)
{
    if (needles.isEmpty())
        return true;
    const QString hay = lower(haystack);
    const QStringList parts = needles.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (hay.contains(part))
            return true;
    }
    return false;
}

QString stripEngine(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("(?i)\\.exe$")), QString());
    text.replace(QRegularExpression(QStringLiteral("(?i)[-_ ]win64[-_ ]shipping.*$")), QString());
    text.replace(QRegularExpression(QStringLiteral("(?i)[-_ ]shipping(_eac|_eos)?$")), QString());
    text.replace(QRegularExpression(QStringLiteral("(?i)[-_ ]dx1[12]$")), QString());
    return text.trimmed();
}

QString collapse(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

QString firstSegment(const QString &title)
{
    const QString cleaned = collapse(title);
    const QStringList separators = {QStringLiteral(" — "), QStringLiteral(" - "), QStringLiteral(" | "),
                                    QStringLiteral(" · ")};
    for (const QString &separator : separators) {
        const int at = cleaned.indexOf(separator);
        if (at >= 2 && at <= 48)
            return cleaned.left(at).trimmed();
    }
    return cleaned;
}

bool usableName(const QString &text)
{
    const QString name = collapse(text);
    if (name.size() < 2 || name.size() > 64)
        return false;
    const QString folded = lower(name);
    if (folded.contains(QLatin1String("platform")) || folded.contains(QLatin1String("binary"))
        || folded.endsWith(QLatin1String(".exe")) || folded.contains(QLatin1String("setup")))
        return false;
    return true;
}

QString slug(const QString &text)
{
    QString out;
    for (const QChar ch : lower(text)) {
        if (ch.isLetterOrNumber())
            out.append(ch);
        else if (!out.endsWith(QLatin1Char('-')))
            out.append(QLatin1Char('-'));
    }
    while (out.endsWith(QLatin1Char('-')))
        out.chop(1);
    if (out.size() > 40)
        out.truncate(40);
    return out;
}

const Rule *findRule(const QString &exe, const QString &title, const QString &product)
{
    const QString name = lower(exe);
    for (const Rule &rule : kRules) {
        const QStringList exes = QString::fromLatin1(rule.exes).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        if (!exes.contains(name))
            continue;
        if (rule.needles[0] == '\0')
            return &rule;
        if (containsNeedle(title, QString::fromLatin1(rule.needles))
            || containsNeedle(product, QString::fromLatin1(rule.needles)))
            return &rule;
    }
    return nullptr;
}

const Rule *findByDisplay(const QString &text)
{
    const QString folded = lower(text);
    for (const Rule &rule : kRules) {
        if (folded == lower(QString::fromLatin1(rule.name)))
            return &rule;
    }
    return nullptr;
}

QString cleanTitle(const QString &title)
{
    QString text = stripEngine(firstSegment(title));
    text = collapse(text);
    if (text.size() > 48) {
        text.truncate(48);
        text.remove(QRegularExpression(QStringLiteral("\\s+\\S*$")));
    }
    return text.trimmed();
}

QString cleanExe(const QString &exe)
{
    QString base = exe;
    if (base.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive))
        base.chop(4);
    return stripEngine(base).trimmed();
}

} // namespace

bool AppCatalog::isLauncher(const QString &exeName)
{
    return listed(kLaunchers, int(sizeof(kLaunchers) / sizeof(kLaunchers[0])), exeName);
}

bool AppCatalog::isRuntime(const QString &exeName)
{
    return listed(kRuntimes, int(sizeof(kRuntimes) / sizeof(kRuntimes[0])), exeName);
}

bool AppCatalog::isIgnored(const QString &exeName)
{
    return listed(kIgnored, int(sizeof(kIgnored) / sizeof(kIgnored[0])), exeName);
}

bool AppCatalog::isGame(const QString &stableId)
{
    if (!stableId.startsWith(QStringLiteral("app:")))
        return false;
    const QString id = stableId.mid(4);
    return !listed(kApps, int(sizeof(kApps) / sizeof(kApps[0])), id);
}

AppMatch AppCatalog::resolve(const AppFacts &facts)
{
    AppMatch match;
    const QString exe = facts.exeName.isEmpty() ? QString() : facts.exeName;
    if (const Rule *rule = findRule(exe, facts.windowTitle, facts.productName)) {
        match.displayName = QString::fromLatin1(rule->name);
        match.stableId = QStringLiteral("app:") + QString::fromLatin1(rule->id);
        match.recognized = true;
        return match;
    }
    if (const Rule *named = findByDisplay(facts.productName)) {
        match.displayName = QString::fromLatin1(named->name);
        match.stableId = QStringLiteral("app:") + QString::fromLatin1(named->id);
        match.recognized = true;
        return match;
    }

    const bool runtime = isRuntime(exe);
    QString display;
    if (!runtime && usableName(facts.productName))
        display = collapse(facts.productName);
    else if (!runtime && usableName(facts.fileDescription) && lower(facts.fileDescription) != lower(facts.exeName))
        display = collapse(stripEngine(facts.fileDescription));
    else if (usableName(cleanTitle(facts.windowTitle)))
        display = cleanTitle(facts.windowTitle);
    else
        display = cleanExe(exe);

    if (display.isEmpty())
        display = QStringLiteral("Desktop");
    match.displayName = display;
    if (runtime)
        match.stableId = QStringLiteral("run:") + lower(exe) + QLatin1Char(':') + slug(display);
    else
        match.stableId = QStringLiteral("exe:") + lower(exe);
    match.recognized = false;
    return match;
}

QString AppCatalog::displayForStored(const QString &stored)
{
    const QString text = collapse(stored);
    if (text.isEmpty() || lower(text) == QLatin1String("desktop"))
        return QStringLiteral("Desktop");
    const QString folded = lower(text);
    for (const Alias &alias : kAliases) {
        if (folded == QLatin1String(alias.from))
            return QString::fromLatin1(alias.to);
    }
    if (const Rule *named = findByDisplay(text))
        return QString::fromLatin1(named->name);
    if (!folded.contains(QLatin1Char(' '))) {
        AppFacts asExe;
        asExe.exeName = folded.endsWith(QLatin1String(".exe")) ? folded : folded + QStringLiteral(".exe");
        asExe.windowTitle = text;
        const AppMatch byExe = resolve(asExe);
        if (byExe.recognized)
            return byExe.displayName;
    }

    AppFacts facts;
    if (folded.endsWith(QLatin1String(".exe")) || folded.contains(QLatin1String("win64"))
        || folded.contains(QLatin1String("shipping"))) {
        facts.exeName = folded.endsWith(QLatin1String(".exe")) ? folded : folded + QStringLiteral(".exe");
    }
    facts.windowTitle = text;
    const AppMatch match = resolve(facts);
    if (match.recognized)
        return match.displayName;
    const QString cleaned = cleanTitle(text);
    return cleaned.isEmpty() ? text : cleaned;
}

QString AppCatalog::fileToken(const QString &displayName)
{
    QString token = Paths::sanitizeName(displayName);
    token.replace(QLatin1Char(' '), QLatin1Char('_'));
    return token;
}

} // namespace nuclip
