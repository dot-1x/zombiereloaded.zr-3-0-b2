/*
 * ============================================================================
 *
 *  Zombie:Reloaded
 *
 *  File:          zmarket.inc
 *  Type:          Module
 *  Description:   ZMarket module, provides menu of weapons to buy from.
 *
 *  Copyright (C) 2009  Greyscale, Richard Helgeby
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ============================================================================
 */

/**
 * @section Name of the cookies for ZMarket rebuy/auto-rebuy data.
 */
#define ZMARKET_COOKIE_AUTOREBUY "zr_zmarket_autorebuy"
#define ZMARKET_COOKIE_REBUY "zr_zmarket_rebuy"
/**
 * @endsection
 */

/**
 * Variable to store buyzone offset value.
 */
new g_iToolsInBuyZone;

/**
 * Array to store the client's current weapon type within menu.
 */
new g_iZMarketCurType[MAXPLAYERS + 1];

/**
 * Array handle to store the amount of purchases left for a weapon.
 */
new Handle:g_hZMarketPurchaseCount[MAXPLAYERS + 1];

/**
 * Cookie handle for auti-rebuy.
 */
new Handle:g_hZMarketAutoRebuyCookie = INVALID_HANDLE;

/**
 * Create commands specific to ZMarket.
 */
ZMarketOnCommandsCreate()
{
    // Register ZMarket command.
    RegConsoleCmd(SAYHOOKS_KEYWORD_ZMARKET, ZMarketCommand, "Opens custom buymenu.");
}

ZMarketOnCookiesCreate()
{
    // Create all cookies.
    g_hZMarketAutoRebuyCookie = RegClientCookie(ZMARKET_COOKIE_AUTOREBUY, "The toggle state of auto-rebuy.", CookieAccess_Protected);
    
    decl String:rebuycookiename[32];
    decl String:rebuycookiedesc[64];
    
    // x = Weapon slot.
    for (new x = 0; x < WEAPONS_SLOTS_MAX + 1; x++)
    {
        // Format cookie name and description.
        Format(rebuycookiename, sizeof(rebuycookiename), "%s_%d", ZMARKET_COOKIE_REBUY, x);
        Format(rebuycookiedesc, sizeof(rebuycookiedesc), "Current loadout weapon for slot %d", x);
        
        // Register client cookie.
        RegClientCookie(rebuycookiename, rebuycookiedesc, CookieAccess_Protected);
    }
}

/**
 * Find ZMarket-specific offsets here.
 */
ZMarketOnOffsetsFound()
{
    // If offset "m_bInBuyZone" can't be found, then stop the plugin.
    g_iToolsInBuyZone = FindSendPropInfo("CCSPlayer", "m_bInBuyZone");
    if (g_iToolsInBuyZone == -1)
    {
        LogEvent(false, LogType_Fatal, LOG_CORE_EVENTS, LogModule_Weapons, "Offsets", "Offset \"CCSPlayer::m_bInBuyZone\" was not found.");
    }
}

/**
 * Client is joining the server.
 * 
 * @param client    The client index.  
 */
ZMarketClientInit(client)
{
    // If purchase count data exists, then destroy before creating again.
    if (g_hZMarketPurchaseCount[client] != INVALID_HANDLE)
    {
        CloseHandle(g_hZMarketPurchaseCount[client]);
    }
    
    // Create a new array handle to store purchase count data for client.
    g_hZMarketPurchaseCount[client] = CreateTrie();
    
    // Initialize auto-rebuy data.
    decl String:zmarketautorebuy[8];
    GetClientCookie(client, g_hZMarketAutoRebuyCookie, zmarketautorebuy, sizeof(zmarketautorebuy));
    
    // If the cookie is empty, then set the default value.
    if (!zmarketautorebuy[0])
    {
        // Set cookie to false.
        CookiesSetClientCookieBool(client, g_hZMarketAutoRebuyCookie, false);
    }
}

/**
 * Client is leaving the server.
 * 
 * @param client    The client index.
 */
ZMarketOnClientDisconnect(client)
{
    // Destroy ZMarket array data for client.
    if (g_hZMarketPurchaseCount[client] != INVALID_HANDLE)
    {
        CloseHandle(g_hZMarketPurchaseCount[client]);
    }
    
    // Reset handle.
    g_hZMarketPurchaseCount[client] = INVALID_HANDLE;
}

/**
 * Client is spawning into the game.
 * 
 * @param client    The client index.
 */
ZMarketOnClientSpawnPost(client)
{
    // Reset purchase counts for client.
    ZMarketResetPurchaseCount(client);
    
    // If client hasn't spawned into the game yet, then stop.
    if (!IsPlayerAlive(client))
    {
        return;
    }
    
    // If client is a zombie, then stop.
    if (InfectIsClientInfected(client))
    {
        return;
    }
    
    // If auto-rebuy is enabled, then force client to rebuy weapons.
    if (CookiesGetClientCookieBool(client, g_hZMarketAutoRebuyCookie))
    {
        ZMarketRebuy(client, true);
    }
}

/**
 * Reset the purchase count(s) for a client.
 * 
 * @param client    The client index.
 */
ZMarketResetPurchaseCount(client)
{
    // Clear out the trie of all data.
    ClearTrie(g_hZMarketPurchaseCount[client]);
}

/**
 * Set the purchases left for a client of a weapon.
 * 
 * @param client    The client index.
 * @param weapon    The weapon name.
 * @param value     The amount of purchases left for the weapon.
 */
ZMarketSetPurchaseCount(client, const String:weapon[], value, bool:add = false)
{
    // Initialize variable (value is 0)
    new purchasemax;
    
    // If we are adding, then update variable with current ammo value.
    if (add)
    {
        purchasemax = ZMarketGetPurchaseCount(client, weapon);
    }
    
    // Set value in client's trie.
    SetTrieValue(g_hZMarketPurchaseCount[client], weapon, purchasemax + value);
}

/**
 * Get the purchases left for a client of a weapon.
 * 
 * @param client    The client index.
 * @param weapon    The weapon name.
 * @param return    The amount of purchases left for the weapon.
 */
ZMarketGetPurchaseCount(client, const String:weapon[])
{
    // Get value in client's trie.
    new value;
    GetTrieValue(g_hZMarketPurchaseCount[client], weapon, value);
    return value;
}

/**
 * Sends main ZMarket menu to client.
 *  
 * @param client    The client index.
 * @return          True if the menu was sent successfully, false if not.
 */
bool:ZMarketMenuMain(client)
{
    // If weapons module is disabled, then stop.
    new bool:weapons = GetConVarBool(g_hCvarsList[CVAR_WEAPONS]);
    if (!weapons)
    {
        // Telle client feature is disabled.
        TranslationPrintToChat(client, "Feature is disabled");
        return false;
    }
    
    // If ZMarket module is disabled, then stop.
    new bool:zmarket = GetConVarBool(g_hCvarsList[CVAR_WEAPONS_ZMARKET]);
    if (!zmarket)
    {
        // Telle client feature is disabled.
        TranslationPrintToChat(client, "Feature is disabled");
        return false;
    }
    
    // Create menu handle.
    new Handle:menu_zmarket_main = CreateMenu(ZMarketMenuMainHandle);
    
    // Set translation target as client.
    SetGlobalTransTarget(client);
    
    // Set menu title.
    SetMenuTitle(menu_zmarket_main, "%t\n ", "Weapons menu zmarket main title");
    
    // If rebuy is enabled, then add it to the menu.
    new bool:zmarketrebuy = GetConVarBool(g_hCvarsList[CVAR_WEAPONS_ZMARKET_REBUY]);
    new bool:zmarketrebuyauto = GetConVarBool(g_hCvarsList[CVAR_WEAPONS_ZMARKET_REBUY_AUTO]);
    
    // Add "Get Current Loadout" option.
    decl String:getloadout[WEAPONS_MAX_LENGTH];
    decl String:viewloadout[WEAPONS_MAX_LENGTH];
    decl String:rebuy[WEAPONS_MAX_LENGTH];
    decl String:rebuyauto[WEAPONS_MAX_LENGTH];
    decl String:weaponslist[WEAPONS_MAX_LENGTH];
    
    // Get auto-rebuy setting.
    decl String:rebuyautosetting[8];
    ConfigBoolToSetting(CookiesGetClientCookieBool(client, g_hZMarketAutoRebuyCookie), rebuyautosetting, sizeof(rebuyautosetting));
    
    // Format menu options.
    Format(getloadout, sizeof(getloadout), "%t", "Weapons menu zmarket main get loadout");
    Format(viewloadout, sizeof(viewloadout), "%t", "Weapons menu zmarket main view loadout");
    Format(rebuy, sizeof(rebuy), "%t", "Weapons menu zmarket main rebuy");
    Format(rebuyauto, sizeof(rebuyauto), "%t", "Weapons menu zmarket main auto-rebuy", rebuyautosetting);
    Format(weaponslist, sizeof(weaponslist), "%t", "Weapons menu zmarket main weapons list");
    
    // Add formatted options to menu.
    AddMenuItem(menu_zmarket_main, "Get Loadout", getloadout, MenuGetItemDraw(zmarketrebuy || zmarketrebuyauto));
    AddMenuItem(menu_zmarket_main, "View Loadout", viewloadout, MenuGetItemDraw(zmarketrebuy || zmarketrebuyauto));
    AddMenuItem(menu_zmarket_main, "Rebuy", rebuy, MenuGetItemDraw(zmarketrebuy || zmarketrebuyauto));
    AddMenuItem(menu_zmarket_main, "Auto-Rebuy", rebuyauto, MenuGetItemDraw(zmarketrebuyauto));
    AddMenuItem(menu_zmarket_main, "Weapons List", weaponslist);
    
    // Set exit back button.
    SetMenuExitBackButton(menu_zmarket_main, true);
        
    DisplayMenu(menu_zmarket_main, client, MENU_TIME_FOREVER);
    
    // Menu sent successfully.
    return true;
}

/**
 * Called when client selects option in the main ZMarket menu, and handles it.
 *  
 * @param menu_zmarket_main    Handle of the menu being used.
 * @param action                The action done on the menu (see menus.inc, enum MenuAction).
 * @param client                The client index.
 * @param slot                  The slot index selected (starting from 0).
 */ 
public ZMarketMenuMainHandle(Handle:menu_zmarket_main, MenuAction:action, client, slot)
{
    // Client selected an option.
    if (action == MenuAction_Select)
    {
        switch(slot)
        {
            // Get loadout.
            case 0:
            {
                // Gets a client's current loadout.
                ZMarketGetCurrentLoadout(client);
                
                // Resend menu.
                ZMarketMenuMain(client);
            }
            // View loadout.
            case 1:
            {
                // Show a client their current loadout.
                ZMarketMenuLoadout(client);
            }
            // Rebuy.
            case 2:
            {
                // Force client to rebuy weapons.
                ZMarketRebuy(client);
                
                // Resend menu.
                ZMarketMenuMain(client);
            }
            // Auto-rebuy.
            case 3:
            {
                // Toggle the auto-rebuy cookie.
                ZMarketToggleAutoRebuy(client);
                
                // Resend menu.
                ZMarketMenuMain(client);
            }
            // Weapons list.
            case 4:
            {
                // Send the weapon types list.
                ZMarketMenuTypes(client);
            }
        }
    }
    // Client closed the menu.
    if (action == MenuAction_Cancel)
    {
        // Client hit "Back" button.
        if (slot == MenuCancel_ExitBack)
        {
            ZMenuMain(client);
        }
    }
    // Client hit "Exit" button.
    else if (action == MenuAction_End)
    {
        CloseHandle(menu_zmarket_main);
    }
}

/**
 * Sends list of weapons in a client's loadout to the client.
 *  
 * @param client    The client index.
 */
bool:ZMarketMenuLoadout(client)
{
    // Create menu handle.
    new Handle:menu_zmarket_loadout = CreateMenu(ZMarketMenuLoadoutHandle);
    
    // Set translation target as client.
    SetGlobalTransTarget(client);
    
    // Set menu title.
    SetMenuTitle(menu_zmarket_loadout, "%t\n ", "Weapons menu zmarket loadout title");
    
    decl String:primaryweapon[WEAPONS_MAX_LENGTH];
    decl String:secondaryweapon[WEAPONS_MAX_LENGTH];
    decl String:meleeweapon[WEAPONS_MAX_LENGTH];
    decl String:projectileweapon[WEAPONS_MAX_LENGTH];
    decl String:explosiveweapon[WEAPONS_MAX_LENGTH];
    decl String:nvgsweapon[WEAPONS_MAX_LENGTH];
    
    // Transfer cookie values into an array.
    new String:rebuyweapons[WeaponsSlot][WEAPONS_MAX_LENGTH];
    ZMarketCookiesToArray(client, rebuyweapons, WEAPONS_SLOTS_MAX + 1, sizeof(rebuyweapons[]));
    
    // Return the display name for all the client's weapon classname's.
    WeaponsClassnameToDisplay(rebuyweapons[Slot_Primary], sizeof(rebuyweapons[]), primaryweapon, sizeof(primaryweapon));
    WeaponsClassnameToDisplay(rebuyweapons[Slot_Secondary], sizeof(rebuyweapons[]), secondaryweapon, sizeof(secondaryweapon));
    WeaponsClassnameToDisplay(rebuyweapons[Slot_Melee], sizeof(rebuyweapons[]), meleeweapon, sizeof(meleeweapon));
    WeaponsClassnameToDisplay(rebuyweapons[Slot_Projectile], sizeof(rebuyweapons[]), projectileweapon, sizeof(projectileweapon));
    WeaponsClassnameToDisplay(rebuyweapons[Slot_Explosive], sizeof(rebuyweapons[]), explosiveweapon, sizeof(explosiveweapon));
    WeaponsClassnameToDisplay(rebuyweapons[Slot_NVGs], sizeof(rebuyweapons[]), nvgsweapon, sizeof(nvgsweapon));
    
    // Get the empty translation.
    decl String:empty[64];
    Format(empty, sizeof(empty), "%t", "Weapons menu zmarket loadout empty");
    
    // If the client doesn't have a weapon in this slot, then set the weapon to the empty translation.
    if (!primaryweapon[0])
    {
        strcopy(primaryweapon, sizeof(primaryweapon), empty);
    }
    if (!secondaryweapon[0])
    {
        strcopy(secondaryweapon, sizeof(secondaryweapon), empty);
    }
    if (!meleeweapon[0])
    {
        strcopy(meleeweapon, sizeof(meleeweapon), empty);
    }
    if (!projectileweapon[0])
    {
        strcopy(projectileweapon, sizeof(projectileweapon), empty);
    }
    if (!explosiveweapon[0])
    {
        strcopy(explosiveweapon, sizeof(explosiveweapon), empty);
    }
    
    // Copy "Yes/No" to NVGs string.
    decl String:nvgsbool[8];
    ConfigBoolToSetting(bool:nvgsweapon[0], nvgsbool, sizeof(nvgsbool));
    
    decl String:primary[64];
    decl String:secondary[64];
    decl String:melee[64];
    decl String:projectile[64];
    decl String:explosive[64];
    decl String:nvgs[64];
    
    // Format all the lines of the menu.
    Format(primary, sizeof(primary), "%t", "Weapons menu zmarket loadout primary", primaryweapon);
    Format(secondary, sizeof(secondary), "%t", "Weapons menu zmarket loadout secondary", secondaryweapon);
    Format(melee, sizeof(melee), "%t", "Weapons menu zmarket loadout melee", meleeweapon);
    Format(projectile, sizeof(projectile), "%t", "Weapons menu zmarket loadout projectile", projectileweapon);
    Format(explosive, sizeof(explosive), "%t", "Weapons menu zmarket loadout explosive", explosiveweapon);
    Format(nvgs, sizeof(nvgs), "%t", "Weapons menu zmarket loadout nvgs", nvgsbool);
    
    // Add formatted options to menu.
    AddMenuItem(menu_zmarket_loadout, "0", primary, MenuGetItemDraw(!StrEqual(primaryweapon, empty)));
    AddMenuItem(menu_zmarket_loadout, "1", secondary, MenuGetItemDraw(!StrEqual(secondaryweapon, empty)));
    AddMenuItem(menu_zmarket_loadout, "2", melee, MenuGetItemDraw(!StrEqual(meleeweapon, empty)));
    AddMenuItem(menu_zmarket_loadout, "3", projectile, MenuGetItemDraw(!StrEqual(projectileweapon, empty)));
    AddMenuItem(menu_zmarket_loadout, "4", explosive, MenuGetItemDraw(!StrEqual(explosiveweapon, empty)));
    AddMenuItem(menu_zmarket_loadout, "5", nvgs, MenuGetItemDraw(bool:nvgsweapon[0]));
    
    // Set exit back button.
    SetMenuExitBackButton(menu_zmarket_loadout, true);
    
    // Send menu to client.
    DisplayMenu(menu_zmarket_loadout, client, MENU_TIME_FOREVER);
}

/**
 * Allow the user to press the "Back" button to go to main ZMarket menu.
 *  
 * @param menu_zmarket_loadout    Handle of the menu being used.
 * @param action                The action done on the menu (see menus.inc, enum MenuAction).
 * @param client                The client index.
 * @param slot                  The slot index selected (starting from 0).
 */ 
public ZMarketMenuLoadoutHandle(Handle:menu_zmarket_loadout, MenuAction:action, client, slot)
{
    // Client selected an option.
    if (action == MenuAction_Select)
    {
        // Clear rebuy slot.
        ZMarketSetRebuyCookie(client, WeaponsSlot:slot, "");
        
        // Re-send menu.
        ZMarketMenuLoadout(client);
    }
    // Client closed the menu.
    if (action == MenuAction_Cancel)
    {
        // Client hit "Back" button.
        if (slot == MenuCancel_ExitBack)
        {
            ZMarketMenuMain(client);
        }
    }
    // Client hit "Exit" button.
    else if (action == MenuAction_End)
    {
        CloseHandle(menu_zmarket_loadout);
    }
}

/**
 * Sends weapon type list to client.
 *  
 * @param client    The client index.
 */
ZMarketMenuTypes(client)
{
    // Create menu handle.
    new Handle:menu_zmarket_types = CreateMenu(ZMarketMenuTypesHandle);
    
    // Set translation target as client.
    SetGlobalTransTarget(client);
    
    // Set menu title.
    SetMenuTitle(menu_zmarket_types, "%t\n ", "Weapons menu zmarket types title");
    
    decl String:typename[WEAPONS_MAX_LENGTH];
    
    // x = Array index.
    new size = GetArraySize(arrayWeaponTypes);
    for (new x = 0; x < size; x++)
    {
        // Get name of type.
        RestrictWeaponTypeGetName(x, typename, sizeof(typename));
        
        // Add item to menu.
        AddMenuItem(menu_zmarket_types, typename, typename);
    }
    
    // If there are no weapons, add an "(Empty)" line.
    if (size == 0)
    {
        decl String:empty[64];
        Format(empty, sizeof(empty), "%t", "Menu empty");
        
        AddMenuItem(menu_zmarket_types, "empty", empty, ITEMDRAW_DISABLED);
    }
    
    // Set exit back button.
    SetMenuExitBackButton(menu_zmarket_types, true);
        
    DisplayMenu(menu_zmarket_types, client, MENU_TIME_FOREVER);
}

/**
 * Called when client selects option in the weapons list menu, and handles it.
 *  
 * @param menu_zmarket_types    Handle of the menu being used.
 * @param action                The action done on the menu (see menus.inc, enum MenuAction).
 * @param client                The client index.
 * @param slot                  The slot index selected (starting from 0).
 */ 
public ZMarketMenuTypesHandle(Handle:menu_zmarket_types, MenuAction:action, client, slot)
{
    // Client selected an option.
    if (action == MenuAction_Select)
    {
        decl String:weapontype[WEAPONS_MAX_LENGTH];
        GetMenuItem(menu_zmarket_types, slot, weapontype, sizeof(weapontype));
        
        // Get weapon index.
        new weapontypeindex = RestrictTypeToIndex(weapontype);
        
        // If weapon type index is -1, then something went very wrong.
        if (weapontypeindex == -1)
        {
            // Resend menu.
            ZMarketMenuTypes(client);
            
            return;
        }
        
        // Menu slot index is = weapon type index.
        g_iZMarketCurType[client] = weapontypeindex;
        
        // Send weapons of the selected type in a menu to client.
        ZMarketMenuTypeWeapons(client);
    }
    // Client closed the menu.
    if (action == MenuAction_Cancel)
    {
        // Client hit "Back" button.
        if (slot == MenuCancel_ExitBack)
        {
            ZMarketMenuMain(client);
        }
    }
    // Client hit "Exit" button.
    else if (action == MenuAction_End)
    {
        CloseHandle(menu_zmarket_types);
    }
}

/**
 * Sends a list of weapons of a certain type in a menu to the client.
 *  
 * @param client    The client index.
 */
ZMarketMenuTypeWeapons(client)
{
    // Create menu handle.
    new Handle:menu_zmarket_typeweapons = CreateMenu(ZMarketMenuTypeWeaponsHandle);
    
    // Get name of current weapon type.
    decl String:typename[WEAPONS_MAX_LENGTH];
    RestrictWeaponTypeGetName(g_iZMarketCurType[client], typename, sizeof(typename));
    
    // Set menu title.
    SetMenuTitle(menu_zmarket_typeweapons, "%t\n ", "Weapons menu zmarket types weapon type title", typename);
    
    decl String:typeweapon[WEAPONS_MAX_LENGTH];
    decl String:typeweapondisplay[WEAPONS_MAX_LENGTH];
    decl String:display[128];
    
    // Get an array populated with all weapons of the given type.
    new Handle:arrayTypeWeapons;
    new count = RestrictGetTypeWeapons(g_iZMarketCurType[client], arrayTypeWeapons);
    
    // x = Array index.
    for (new x = 0; x < count; x++)
    {
        // Get weapon index to check restricted status of.
        new weaponindex = GetArrayCell(arrayTypeWeapons, x);
        
        // Get name of weapon.
        WeaponsGetName(weaponindex, typeweapon, sizeof(typeweapon));
        
        // Check if client is buying the weapon or ammo for it, and get the price of the item.
        new bool:hasweapon = WeaponsClientHasWeapon(client, typeweapon);
        
        // DO ITEM PRICE STUFF HERE.
        
        new itemprice;
        
        // Do appropriate formatting for the type of item client is buying.
        new WeaponsSlot:slot = WeaponsGetSlot(weaponindex);
        if (!hasweapon || slot == Slot_Invalid || slot == Slot_Projectile)
        {
            // Get the weapon price.
            itemprice = WeaponsGetZMarketPrice(weaponindex);
        }
        else
        {
            // Get the weapon's ammo price.
            itemprice = WeaponsGetAmmoPrice(weaponindex);
            
            // Tell client they are buying ammo.
            Format(typeweapondisplay, sizeof(typeweapondisplay), "%s %t", typeweapon, "Weapons menu zmarket types weapon ammo");
        }
        
        // If the itemprice is invalid, then set to known integer to be later replaced.
        if (itemprice < 0)
        {
            itemprice = -1;
        }
        
        // DO RESTRICTED WEAPON STUFF HERE.
        
        // Get restricted status on the weapon.
        new bool:restricted = RestrictIsWeaponRestricted(weaponindex);
        
        // If weapon is restricted then format "[]" around it.
        strcopy(typeweapondisplay, sizeof(typeweapondisplay), typeweapon);
        if (restricted)
        {
            Format(typeweapondisplay, sizeof(typeweapondisplay), "[%s]", typeweapon);
        }
        
        // DO PURCHASE MAX STUFF HERE.
        
        // Get the purchase count information for this weapon.
        new purchasemax = WeaponsGetZMarketPurchaseMax(weaponindex);
        new purchasecount = ZMarketGetPurchaseCount(client, typeweapon);
        new purchasesleft = purchasemax - purchasecount;
        
        // If the purchases left for the weapon goes below 0, fix it.
        if (purchasesleft < 0)
        {
            purchasesleft = 0;
        }
        
        // If max purchases is disabled for the weapon, then set as known integer to be later replaced.
        if (purchasemax <= 0)
        {
            purchasesleft = -1;
        }
        
        // Format all the information together.
        
        // Format price onto the menu entry.
        Format(display, sizeof(display), "%t", "Weapons menu zmarket types weapon info", typeweapondisplay, itemprice, purchasesleft);
        
        // Remove the known invalid number from the string, and replace with N/A, and remove currency symbol.
        ReplaceString(display, sizeof(display), "$-1", "N/A");
        ReplaceString(display, sizeof(display), "-1", "N/A");
        
        // Disable option if it isn't toggleable.
        new bool:itemdraw = ((itemprice > -1) && !restricted && (purchasemax <= 0 || purchasesleft > 0));
        AddMenuItem(menu_zmarket_typeweapons, typeweapon, display, MenuGetItemDraw(itemdraw));
    }
    
    // Destroy the array handle.
    CloseHandle(arrayTypeWeapons);
    
    // Set menu back button.
    SetMenuExitBackButton(menu_zmarket_typeweapons, true);
    
    // Display menu to client.
    DisplayMenu(menu_zmarket_typeweapons, client, MENU_TIME_FOREVER);
}

/**
 * Called when client selects option in the weapon group menu, and handles it.
 *  
 * @param menu_zmarket_typeweapons  Handle of the menu being used.
 * @param action                    The action done on the menu (see menus.inc, enum MenuAction).
 * @param client                    The client index.
 * @param slot                      The slot index selected (starting from 0).
 */ 
public ZMarketMenuTypeWeaponsHandle(Handle:menu_zmarket_typeweapons, MenuAction:action, client, slot)
{
    // Client selected an option.
    if (action == MenuAction_Select)
    {
        // Get name of current weapon type.
        decl String:typename[WEAPONS_MAX_LENGTH];
        RestrictWeaponTypeGetName(g_iZMarketCurType[client], typename, sizeof(typename));
        
        decl String:typeweapon[WEAPONS_MAX_LENGTH];
        GetMenuItem(menu_zmarket_typeweapons, slot, typeweapon, sizeof(typeweapon));
        
        // Equip weapon on client.
        ZMarketEquip(client, typeweapon);
        
        // Open types menu.
        ZMarketMenuTypes(client);
    }
    // Client closed the menu.
    if (action == MenuAction_Cancel)
    {
        // Client hit "Back" button.
        if (slot == MenuCancel_ExitBack)
        {
            ZMarketMenuTypes(client);
        }
    }
    // Client hit "Exit" button.
    else if (action == MenuAction_End)
    {
        CloseHandle(menu_zmarket_typeweapons);
    }
}

/**
 * Equip a weapon on a client.
 * 
 * @param client    The client index.
 * @param weapon    The weapon to equip. (must be in weapons config file)
 * @param rebuy     (Optional) If client is rebuying, ammo will be ignored.
 */
stock bool:ZMarketEquip(client, const String:weapon[], bool:rebuy = false)
{
    // Get weapon index.
    new weaponindex = WeaponsNameToIndex(weapon);
    
    // If weapon index is -1, then something went very wrong.
    if (weaponindex == -1)
    {
        LogEvent(false, LogType_Error, LOG_CORE_EVENTS, LogModule_Weapons, "ZMarket Equip", "Invalid weapon (%s) selected from the ZMarket menu.", weapon);
        return false;
    }
    
    // Get slot index of the weapon being purchased.
    new WeaponsSlot:slot = WeaponsGetSlot(weaponindex);
    
    // If client is a zombie, then update weapon loadout and stop.
    if (InfectIsClientInfected(client))
    {
        // Update cookie with new weapon.
        ZMarketSetRebuyCookie(client, slot, weapon);
        
        TranslationPrintToChat(client, "Weapons zmarket updated loadout");
        return false;
    }
    
    new bool:zmarketbuyzone = GetConVarBool(g_hCvarsList[CVAR_WEAPONS_ZMARKET_BUYZONE]);
    if (!rebuy && zmarketbuyzone && !ZMarketIsClientInBuyZone(client))
    {
        // Update cookie with new weapon.
        ZMarketSetRebuyCookie(client, slot, weapon);
        
        TranslationPrintToChat(client, "Weapons zmarket updated loadout");
        return false;
    }
    
    // Get the appropriate price of the item being purchased.
    new bool:hasweapon = WeaponsClientHasWeapon(client, weapon);
    new itemprice = (hasweapon && slot != Slot_Invalid && slot != Slot_Projectile) ? WeaponsGetAmmoPrice(weaponindex) : WeaponsGetZMarketPrice(weaponindex);
    
    // If the weapon price is below 0, then something went wrong.
    if (itemprice < 0)
    {
        return false;
    }
    
    // Copy the 'weapon' variable into a local variable for indexing.
    decl String:weaponname[WEAPONS_MAX_LENGTH];
    strcopy(weaponname, sizeof(weaponname), weapon);
    
    // Get the display name for the weapon.
    decl String:weapondisplay[WEAPONS_MAX_LENGTH];
    WeaponsClassnameToDisplay(weaponname, sizeof(weaponname), weapondisplay, sizeof(weapondisplay));
    
    // Check to make sure the weapon isn't restricted.
    new bool:restricted = RestrictIsWeaponRestricted(weaponindex);
    if (restricted)
    {
        TranslationPrintToChat(client, "Weapon is restricted", weapondisplay);
        return false;
    }
    
    // Get the purchase count information for this weapon.
    new purchasemax = WeaponsGetZMarketPurchaseMax(weaponindex);
    new purchasecount = ZMarketGetPurchaseCount(client, weapon);
    new purchasesleft = purchasemax - purchasecount;
    if (purchasemax > 0 && purchasesleft <= 0)
    {
        TranslationPrintToChat(client, "Weapons zmarket purchase max", weapondisplay, purchasemax);
        return false;
    }
    
    // Get client's current money.
    new cash = AccountGetClientCash(client);
    
    // If client doesn't have enough money, tell them, resend menu, and then stop.
    if (cash < itemprice)
    {
        // Update cookie with new weapon.
        ZMarketSetRebuyCookie(client, slot, weapon);
        
        TranslationPrintToChat(client, "Weapons zmarket updated loadout");
        return false;
    }
    
    // Set client's new cash after purchase.
    AccountSetClientCash(client, cash - itemprice);
    
    // Get a list of the client's current weapons.
    new weapons[WeaponsSlot];
    WeaponsGetClientWeapons(client, weapons);
    
    // Check if client is buying the weapon or ammo for it.
    if (!hasweapon || slot == Slot_Projectile || slot == Slot_NVGs)
    {
        
        // If the item is a projectile or NVGs, then skip.
        if (slot != Slot_Projectile && slot != Slot_NVGs)
        {
            // If there is already a weapon in the slot, then force client to drop it.
            if (weapons[slot] > -1)
            {
                // Force client to drop the weapon.
                WeaponsForceClientDrop(client, weapons[slot]);
            }
        }
        
        // Format name into entity name.
        decl String:weaponentity[WEAPONS_MAX_LENGTH];
        
        // If this is the NVGs slot, then format "item_" in front instead of "weapon_".
        if (slot == Slot_NVGs)
        {
            Format(weaponentity, sizeof(weaponentity), "item_%s", weapon);
        }
        else
        {
            Format(weaponentity, sizeof(weaponentity), "weapon_%s", weapon);
        }
        
        // Give client the weapon.
        GivePlayerItem(client, weaponentity);
        
        // Update cookie with new weapon.
        ZMarketSetRebuyCookie(client, slot, weapon);
        
        // Add 1 to the client's purchase count.
        ZMarketSetPurchaseCount(client, weapon, 1, true);
        
        if (slot != Slot_Projectile && slot != Slot_NVGs)
        {
            // Tell client they bought a weapon.
            TranslationPrintToChat(client, "Weapons zmarket purchase", weapondisplay);
        }
    }
    else if (!rebuy)
    {
        // Get ammo type and stop if it's invalid.
        decl String:weaponammo[WEAPONS_MAX_LENGTH];
        WeaponsGetAmmoType(weaponindex, weaponammo, sizeof(weaponammo));
        if (!weaponammo[0])
        {
            return false;
        }
        
        // Give ammo and preserve client's clip ammo value.
        new clip = WeaponAmmoGetAmmo(weapons[slot], true);
        GivePlayerItem(client, weaponammo);
        WeaponAmmoSetAmmo(weapons[slot], true, clip);
    }
    
    return true;
}

/**
 * Gets the client's current weapons and puts them in the rebuy cache.
 * 
 * @param client    The client index.
 * @return          True if the menu sent successfully, false if not.
 */
bool:ZMarketGetCurrentLoadout(client)
{
    // If client is dead, then tell them they must be alive and stop.
    if (!IsPlayerAlive(client))
    {
        TranslationPrintToChat(client, "Must be alive");
        return false;
    }
    
    // If client is a zombie, then tell them they must be human and stop.
    if (!IsPlayerAlive(client))
    {
        TranslationPrintToChat(client, "Must be human");
        return false;
    }
    
    // Get all of client's current weapons.
    new weapons[WeaponsSlot];
    WeaponsGetClientWeapons(client, weapons);
    
    decl String:weaponname[WEAPONS_MAX_LENGTH];
    
    // x = Weapon slot.
    for (new x = 0; x < WEAPONS_SLOTS_MAX; x++)
    {
        // If slot is empty, then clear this slot in rebuy cache and stop.
        if (weapons[x] == -1)
        {
            // Empty rebuy slot.
            ZMarketSetRebuyCookie(client, WeaponsSlot:x, "");
            continue;
        }
        
        // Get the name of the weapon.
        GetEdictClassname(weapons[x], weaponname, sizeof(weaponname));
        ReplaceString(weaponname, sizeof(weaponname), "weapon_", "");
        
        // Copy the name to the rebuy cache.
        ZMarketSetRebuyCookie(client, WeaponsSlot:x, weaponname);
    }
    
    // Update nightvision ownership.
    new bool:nightvision = ToolsGetClientNightVision(client);
    if (!nightvision)
    {
        // Empty rebuy slot.
        ZMarketSetRebuyCookie(client, Slot_NVGs, "");
    }
    else
    {
        ZMarketSetRebuyCookie(client, Slot_NVGs, "nvgs");
    }
    
    // Tell client their loadout has been updated.
    TranslationPrintToChat(client, "Weapons zmarket get current loadout");
    
    return true;
}

/**
 * Transfer array values to rebuy cookies.
 * 
 * @param client        The client index.
 * @param rebuyweapons  The string array to copy results from.
 * @param maxweapons    The max amount of weapons in the array.
 * @param maxlen        The max length of each cookie result.
 */
stock ZMarketArrayToCookies(client, String:rebuyweapons[WeaponsSlot][], maxweapons, maxlen)
{
    decl String:rebuycookiename[32];
    new Handle:rebuycookie;
    
    // x = Weapon slot.
    for (new x = 0; x < maxweapons; x++)
    {
        // Format cookie name.
        Format(rebuycookiename, sizeof(rebuycookiename), "%s_%d", ZMARKET_COOKIE_REBUY, x);
        
        // Find cookie handle, and retrieve its value.
        rebuycookie = FindClientCookie(rebuycookiename);
        SetClientCookie(client, rebuycookie, rebuyweapons[x]);
    }
}

/**
 * Transfer rebuy cookies to an array for easier access.
 * 
 * @param client        The client index.
 * @param rebuyweapons  The string array to copy results to
 * @param maxweapons    The max amount of weapons in the array.
 * @param maxlen        The max length of each cookie result.
 */
stock ZMarketCookiesToArray(client, String:rebuyweapons[WeaponsSlot][], maxweapons, maxlen)
{
    decl String:rebuycookiename[32];
    new Handle:rebuycookie;
    
    // x = Weapon slot.
    for (new x = 0; x < maxweapons; x++)
    {
        // Format cookie name.
        Format(rebuycookiename, sizeof(rebuycookiename), "%s_%d", ZMARKET_COOKIE_REBUY, x);
        
        // Find cookie handle, and retrieve its value.
        rebuycookie = FindClientCookie(rebuycookiename);
        GetClientCookie(client, rebuycookie, rebuyweapons[x], maxlen);
    }
}

/**
 * Set a weapon slot cookie given the slot and value.
 * 
 * @param client    The client index.
 * @param slot      The weapon slot to set value to.
 * @param value     The value (weaponname) of the slot.
 */
stock ZMarketSetRebuyCookie(client, WeaponsSlot:slot, const String:value[])
{
    // Format cookie name.
    decl String:rebuycookiename[32];
    Format(rebuycookiename, sizeof(rebuycookiename), "%s_%d", ZMARKET_COOKIE_REBUY, _:slot);
    
    // Find cookie handle, and retrieve its value.
    new Handle:rebuycookie = FindClientCookie(rebuycookiename);
    if (rebuycookie == INVALID_HANDLE)
    {
        return;
    }
    
    // Set weapon to cookie.
    SetClientCookie(client, rebuycookie, value);
}

/**
 * Force a client to rebuy their weapons.
 * 
 * @param client    The client index.
 */
ZMarketRebuy(client, bool:autorebuy = false)
{
    // If client is a zombie, then stop.
    if (InfectIsClientInfected(client))
    {
        TranslationPrintToChat(client, "Must be human");
        return;
    }
    
    new bool:zmarketbuyzone = GetConVarBool(g_hCvarsList[CVAR_WEAPONS_ZMARKET_BUYZONE]);
    if (!autorebuy && zmarketbuyzone && !ZMarketIsClientInBuyZone(client))
    {
        TranslationPrintToChat(client, "Weapons zmarket buyzone");
        return;
    }
    
    // Transfer cookie values into an array.
    new String:rebuyweapons[WeaponsSlot][WEAPONS_MAX_LENGTH];
    ZMarketCookiesToArray(client, rebuyweapons, WEAPONS_SLOTS_MAX + 1, sizeof(rebuyweapons[]));
    
    // x = Weapon slot.
    for (new x = 0; x < WEAPONS_SLOTS_MAX + 1; x++)
    {
        // If slot is empty, then stop.
        if (!rebuyweapons[x][0])
        {
            continue;
        }
        
        ZMarketEquip(client, rebuyweapons[x], true);
    }
}

/**
 * Toggle auto-rebuy on a client.
 * 
 * @param client    The client index.
 */
ZMarketToggleAutoRebuy(client)
{
    // If auto-rebuy is disabled, then stop.
    new bool:zmarketrebuyauto = GetConVarBool(g_hCvarsList[CVAR_WEAPONS_ZMARKET_REBUY_AUTO]);
    if (!zmarketrebuyauto)
    {
        return;
    }
    
    // Get the cookie value.
    new bool:autorebuystate = CookiesGetClientCookieBool(client, g_hZMarketAutoRebuyCookie);
    
    // Toggle the value.
    CookiesSetClientCookieBool(client, g_hZMarketAutoRebuyCookie, !autorebuystate);
    
    // If auto-rebuy was enabled, then tell client it has been disabled.
    if (autorebuystate)
    {
        TranslationPrintToChat(client, "Weapons zmarket auto-rebuy toggle off");
    }
    // If auto-rebuy was disabled, then tell client it has been enabled.
    else
    {
        TranslationPrintToChat(client, "Weapons zmarket auto-rebuy toggle on");
    }
}

/**
 * Command callback (zmarket)
 * Opens custom buymenu.
 * 
 * @param client    The client index.
 * @param argc      Argument count.
 */
public Action:ZMarketCommand(client, argc)
{
    // If client is console, then stop and tell them this feature is for players only.
    if (ZRIsConsole(client))
    {
        TranslationPrintToServer("Must be player");
        return Plugin_Handled;
    }
    
    // Send ZMarket menu.
    ZMarketMenuMain(client);
    
    // This stops the "Unknown command" message in client's console.
    return Plugin_Handled;
}

/**
 * Checks if a client is in a buyzone.
 * 
 * @param client    The client index.
 */
stock bool:ZMarketIsClientInBuyZone(client)
{
    // Return if client is in buyzone.
    return bool:GetEntData(client, g_iToolsInBuyZone);
}