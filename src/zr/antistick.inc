/*
 * ============================================================================
 *
 *  Zombie:Reloaded
 *
 *  File:          antistick.inc
 *  Type:          Module
 *  Description:   Antistick system.
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
 * @section Collision values.
 */
#define ANTISTICK_COLLISIONS_OFF 2
#define ANTISTICK_COLLISIONS_ON 5

#define COLLISION_GROUP_NONE                0   /** Default; collides with static and dynamic objects. */
#define COLLISION_GROUP_DEBRIS              1   /** Collides with nothing but world and static stuff. */
#define COLLISION_GROUP_DEBRIS_TRIGGER      2   /** Same as debris, but hits triggers. */
#define COLLISION_GROUP_INTERACTIVE_DEBRIS  3   /** Collides with everything except other interactive debris or debris. */
#define COLLISION_GROUP_INTERACTIVE         4   /** Collides with everything except interactive debris or debris. */
#define COLLISION_GROUP_PLAYER              5   /** This is the default behavior expected for most prop_physics. */
#define COLLISION_GROUP_BREAKABLE_GLASS     6   /** Special group for glass debris. */
#define COLLISION_GROUP_VEHICLE             7   /** Collision group for driveable vehicles. */
#define COLLISION_GROUP_PLAYER_MOVEMENT     8   /** For HL2, same as Collision_Group_Player. */
#define COLLISION_GROUP_NPC                 9   /** Generic NPC group. */
#define COLLISION_GROUP_IN_VEHICLE          10  /** For any entity inside a vehicle. */
#define COLLISION_GROUP_WEAPON              11  /** For any weapons that need collision detection. */
#define COLLISION_GROUP_VEHICLE_CLIP        12  /** Vehicle clip brush to restrict vehicle movement. */
#define COLLISION_GROUP_PROJECTILE          13  /** Projectiles. */
#define COLLISION_GROUP_DOOR_BLOCKER        14  /** Blocks entities not permitted to get near moving doors. */
#define COLLISION_GROUP_PASSABLE_DOOR       15  /** Doors that the player shouldn't collide with. */
#define COLLISION_GROUP_DISSOLVING          16  /** Things that are dissolving are in this group. */
#define COLLISION_GROUP_PUSHAWAY            17  /** Nonsolid on client and server, pushaway in player code. */
#define COLLISION_GROUP_NPC_ACTOR           18  /** Used so NPCs in scripts ignore the player. */
/**
 * @endsection
 */

/**
 * Default player hull width.
 */
#define ANTISTICK_DEFAULT_HULL_WIDTH GetConVarFloat(g_hCvarsList[CVAR_ANTISTICK_DEFAULT_WIDTH])

/**
 * Handle to keyvalue structure where data is stored.
 */
new Handle:g_kvAntiStick = INVALID_HANDLE;

/**
 * Stores "StartTouch" HookID's for each client.
 */
new g_iStartTouchHookID[MAXPLAYERS + 1] = {-1, ...};

/**
 * List of components that make up the model's rectangular boundaries.
 * 
 * F = Front
 * B = Back
 * L = Left
 * R = Right
 * U = Upper
 * D = Down
 */
enum AntiStickBoxBound
{
    BoxBound_FUR = 0, /** Front upper right */
    BoxBound_FUL,     /** etc.. */
    BoxBound_FDR,
    BoxBound_FDL,
    BoxBound_BUR,
    BoxBound_BUL,
    BoxBound_BDR,
    BoxBound_BDL,
}
/**
 * Create commands related to antistick here.
 */
AntiStickOnCommandsCreate()
{
    // Create public command to list model data.
    RegConsoleCmd("zr_antistick_list_models", AntiStickListModelsCommand, "Lists all players and their model data in console. Usage: zr_antistick_list_models");
    
    // Create admin command to set model hull width.
    RegAdminCmd("zr_antistick_set_width", AntiStickSetWidthCommand, ADMFLAG_GENERIC, "Sets the width of a model's hull. (See zr_antistick_list_models) Usage: zr_antistick_set_width <model/player> <width>");
    
    RegConsoleCmd("zr_antistick_dump_group", AntiStickDumpGroupCommand, "Dumps collision group data on one or more players. Usage zr_antistick_dump_group [#userid|name]");
}

/**
 * Creates/loads antistick data file.
 */
AntiStickLoad()
{
    // Create antistick keyvalues if it hasn't been created yet.
    if (g_kvAntiStick == INVALID_HANDLE)
    {
        g_kvAntiStick = CreateKeyValues("antistick");
    }
    
    // Initialize keyvalues.
    if (!AntiStickLoadData())
    {
        AntiStickSaveData();
    }
}

/**
 * Client is joining the server.
 * 
 * @param client    The client index.
 */
AntiStickClientInit(client)
{
    // Hook "StartTouch" and "EndTouch" on client.
    g_iStartTouchHookID[client] = ZRTools_HookStartTouch(client, AntiStickStartTouch);
}

/**
 * Unhook StartTouch and EndTouch function on a client.
 * 
 * @param client    The client index.
 */
AntiStickOnClientDisconnect(client)
{
    // Unhook "StartTouch" callback, and reset variable.
    if (g_iStartTouchHookID[client] != -1)
    {
        ZRTools_UnhookStartTouch(g_iStartTouchHookID[client]);
        g_iStartTouchHookID[client] = -1;
    }
}

/**
 * Load antistick data from file.
 * 
 * @return  True if loaded successfully, false if file wasn't found.  
 */
stock bool:AntiStickLoadData()
{
    // Get cvar's path.
    decl String:filepath[PLATFORM_MAX_PATH];
    GetConVarString(g_hCvarsList[CVAR_ANTISTICK_FILE_PATH], filepath, sizeof(filepath));
    
    // Build full path in return string.
    decl String:fullpath[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, fullpath, PLATFORM_MAX_PATH, filepath);
    
    // Log action to game events.
    LogEvent(false, LogType_Normal, LOG_GAME_EVENTS, LogModule_AntiStick, "Loaded Data", "Antistick data has been loaded from \"%s\"", fullpath);
    
    // Retrieve keyvalue data from a file, and store in server's memory.
    KvRewind(g_kvAntiStick);
    return FileToKeyValues(g_kvAntiStick, fullpath);
}

/**
 * Save antistick data to file.
 */
stock AntiStickSaveData()
{
    // Get cvar's path.
    decl String:filepath[PLATFORM_MAX_PATH];
    GetConVarString(g_hCvarsList[CVAR_ANTISTICK_FILE_PATH], filepath, sizeof(filepath));
    
    // Build full path in return string.
    decl String:fullpath[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, fullpath, PLATFORM_MAX_PATH, filepath);
    
    // Log action to game events.
    LogEvent(false, LogType_Normal, LOG_GAME_EVENTS, LogModule_AntiStick, "Saved Data", "Antistick data has been saved to \"%s\"", fullpath);
    
    // Dump keyvalue structure into a file from the server's memory.
    KvRewind(g_kvAntiStick);
    KeyValuesToFile(g_kvAntiStick, fullpath);
}

/**
 * Get hull width on a client's model. (or return default)
 * 
 * @param client    The client index.
 * @param model     If a client index of 0 is given, this model is used.
 */
stock Float:AntiStickGetModelHullWidth(client, const String:model[] = "")
{
    decl String:clientmodel[PLATFORM_MAX_PATH];
    
    if (ZRIsClientValid(client))
    {
        // Get client's model.
        GetClientModel(client, clientmodel, sizeof(clientmodel));
    }
    else
    {
        // Copy given model to 'clientmodel.'
        strcopy(clientmodel, sizeof(clientmodel), model);
    }
    
    // Find model in antistick data.
    KvRewind(g_kvAntiStick);
    if (KvJumpToKey(g_kvAntiStick, clientmodel))
    {
        // Return value from file.
        return KvGetFloat(g_kvAntiStick, "hull_width", ANTISTICK_DEFAULT_HULL_WIDTH);
    }
    else
    {
        // Return default CS:S hull width.
        return ANTISTICK_DEFAULT_HULL_WIDTH;
    }
}

/**
 * Set hull width on a client's model.
 * 
 * @param client        The client index.
 * @param model     If a client index of 0 is given, this model is used. 
 * @param hull_width    The width of the model hull.
 */
stock AntiStickSetModelHullWidth(client, const String:model[] = "", Float:hull_width)
{
    decl String:clientmodel[PLATFORM_MAX_PATH];
    
    if (ZRIsClientValid(client))
    {
        // Get client's model.
        GetClientModel(client, clientmodel, sizeof(clientmodel));
    }
    else
    {
        // Copy given model to 'clientmodel.'
        strcopy(clientmodel, sizeof(clientmodel), model);
    }
    
    // Replace "/" with "-" because a slash indicates a new level in keyvalues.
    ReplaceString(clientmodel, sizeof(clientmodel), "/", "-");
    
    // Find model in antistick data.
    KvRewind(g_kvAntiStick);
    
    // Create key if it doesn't already exist.
    KvJumpToKey(g_kvAntiStick, clientmodel, true);
    
    // Set value.
    KvSetFloat(g_kvAntiStick, "hull_width", hull_width);
}

/**
 * Callback function for StartTouch.
 * 
 * @param client        The client index.
 * @param entity        The entity index of the entity being touched.
 */
public ZRTools_Action:AntiStickStartTouch(client, entity)
{
    // If antistick is disabled, then stop.
    new bool:antistick = GetConVarBool(g_hCvarsList[CVAR_ANTISTICK]);
    if (!antistick)
    {
        return;
    }
    
    // If client isn't in-game, then stop.
    if (!IsClientInGame(client))
    {
        return;
    }
    
    // If client is touching themselves, then leave them alone :P
    if (client == entity)
    {
        return;
    }
    
    // If touched entity isn't a valid client, then stop.
    if (!ZRIsClientValid(entity))
    {
        return;
    }
    
    // If the clients aren't colliding, then stop.
    if (!AntiStickIsModelBoxColliding(client, entity))
    {
        return;
    }
        
    // From this point we know that client and entity is more or less within eachother.
    LogEvent(false, LogType_Normal, LOG_DEBUG, LogModule_AntiStick, "Collision", "Player \"%N\" and \"%N\" are intersecting. Removing collisions.", client, entity);
    
    // Get current collision groups of client and entity.
    new clientcollisiongroup = AntiStickGetCollisionGroup(client);
    new entitycollisiongroup = AntiStickGetCollisionGroup(entity);
    
    // Note: If zombies get stuck on infection or stuck in a teleport, they'll
    //       get the COLLISION_GROUP_PUSHAWAY collision group, so check this
    //       one too.
    
    // Check if the client have collisions enabled, or have the pushaway collision group.
    if (clientcollisiongroup == ANTISTICK_COLLISIONS_ON || clientcollisiongroup == COLLISION_GROUP_PUSHAWAY)
    {
        // Disable collisions to unstick, and start timers to re-solidify.
        AntiStickSetCollisionGroup(client, ANTISTICK_COLLISIONS_OFF);
        CreateTimer(0.0, AntiStickSolidifyTimer, client, TIMER_FLAG_NO_MAPCHANGE | TIMER_REPEAT);
    }
    
    // Check if the entity have collisions enabled, or have the pushaway collision group.
    if (entitycollisiongroup == ANTISTICK_COLLISIONS_ON || entitycollisiongroup == COLLISION_GROUP_PUSHAWAY)
    {
        AntiStickSetCollisionGroup(entity, ANTISTICK_COLLISIONS_OFF);
        CreateTimer(0.0, AntiStickSolidifyTimer, entity, TIMER_FLAG_NO_MAPCHANGE | TIMER_REPEAT);
    }
}

/**
 * Callback for solidify timer.
 * 
 * @param client        The client index.
 */
public Action:AntiStickSolidifyTimer(Handle:timer, any:client)
{
    // If client has left, then stop.
    if (!IsClientInGame(client))
    {
        return Plugin_Stop;
    }
    
    // If the client is dead, then stop.
    if (!IsPlayerAlive(client))
    {
        return Plugin_Stop;
    }
    
    // If the client's collisions are already on, then stop.
    if (AntiStickGetCollisionGroup(client) == ANTISTICK_COLLISIONS_ON)
    {
        return Plugin_Stop;
    }
    
    // Loop through all clients and check if client is stuck in them.
    for (new x = 1; x <= MaxClients; x++)
    {
        // If client isn't connected or in-game, then skip it.
        if (!IsClientConnected(x) || !IsClientInGame(x))
        {
            continue;
        }
        
        // If the client is dead, then skip it.
        if (!IsPlayerAlive(x))
        {
            continue;
        }
        
        // Don't compare the same clients.
        if (client == x)
        {
            continue;
        }
        
        // If the client is colliding with a client, then allow timer to continue.
        if (AntiStickIsModelBoxColliding(client, x))
        {
            return Plugin_Continue;
        }
    }
    
    // Change collisions back to normal.
    AntiStickSetCollisionGroup(client, ANTISTICK_COLLISIONS_ON);
    
    // Debug message. May be useful when calibrating antistick.
    LogEvent(false, LogType_Normal, LOG_DEBUG, LogModule_AntiStick, "Collision", "Player \"%N\" is no longer intersecting anyone. Applying normal collisions.", client);
    
    return Plugin_Stop;
}

/**
 * Build the model box by finding all vertices.
 * 
 * @param client        The client index.
 * @param boundaries    Array with 'AntiStickBoxBounds' for indexes to return bounds into.
 * @param width         The width of the model box.
 */
stock AntiStickBuildModelBox(client, Float:boundaries[AntiStickBoxBound][3], Float:width)
{
    new Float:clientloc[3];
    new Float:twistang[3];
    new Float:cornerang[3];
    new Float:sideloc[3];
    new Float:finalloc[4][3];
    
    // Get needed vector info.
    GetClientAbsOrigin(client, clientloc);
    
    // Set the pitch to 0.
    twistang[1] = 90.0;
    cornerang[1] = 0.0;
    
    for (new x = 0; x < 4; x++)
    {
        // Jump to point on player's left side.
        AntiStickJumpToPoint(clientloc, twistang, width / 2, sideloc);
        
        // From this point, jump to the corner, which would be half the width from the middle of a side.
        AntiStickJumpToPoint(sideloc, cornerang, width / 2, finalloc[x]);
        
        // Twist 90 degrees to find next side/corner.
        twistang[1] += 90.0;
        cornerang[1] += 90.0;
        
        // Fix angles.
        if (twistang[1] > 180.0)
        {
            twistang[1] -= 360.0;
        }
        
        if (cornerang[1] > 180.0)
        {
            cornerang[1] -= 360.0;
        }
    }
    
    // Copy all horizontal model box data to array.
    boundaries[BoxBound_FUR][0] = finalloc[3][0];
    boundaries[BoxBound_FUR][1] = finalloc[3][1];
    boundaries[BoxBound_FUL][0] = finalloc[0][0];
    boundaries[BoxBound_FUL][1] = finalloc[0][1];
    boundaries[BoxBound_FDR][0] = finalloc[3][0];
    boundaries[BoxBound_FDR][1] = finalloc[3][1];
    boundaries[BoxBound_FDL][0] = finalloc[0][0];
    boundaries[BoxBound_FDL][1] = finalloc[0][1];
    boundaries[BoxBound_BUR][0] = finalloc[2][0];
    boundaries[BoxBound_BUR][1] = finalloc[2][1];
    boundaries[BoxBound_BUL][0] = finalloc[1][0];
    boundaries[BoxBound_BUL][1] = finalloc[1][1];
    boundaries[BoxBound_BDR][0] = finalloc[2][0];
    boundaries[BoxBound_BDR][1] = finalloc[2][1];
    boundaries[BoxBound_BDL][0] = finalloc[1][0];
    boundaries[BoxBound_BDL][1] = finalloc[1][1];
    
    // Set Z bounds.
    new Float:eyeloc[3];
    GetClientEyePosition(client, eyeloc);
    
    boundaries[BoxBound_FUR][2] = eyeloc[2];
    boundaries[BoxBound_FUL][2] = eyeloc[2];
    boundaries[BoxBound_FDR][2] = clientloc[2] + 15.0;
    boundaries[BoxBound_FDL][2] = clientloc[2] + 15.0;
    boundaries[BoxBound_BUR][2] = eyeloc[2];
    boundaries[BoxBound_BUL][2] = eyeloc[2];
    boundaries[BoxBound_BDR][2] = clientloc[2] + 15.0;
    boundaries[BoxBound_BDL][2] = clientloc[2] + 15.0;
}

/**
 * Jumps from a point to another based off angle and distance.
 * 
 * @param vec       Point to jump from.
 * @param ang       Angle to base jump off of.
 * @param distance  Distance to jump
 * @param result    Resultant point.
 */
stock AntiStickJumpToPoint(const Float:vec[3], const Float:ang[3], Float:distance, Float:result[3])
{
    new Float:viewvec[3];
    
    // Turn client angle, into a vector.
    GetAngleVectors(ang, viewvec, NULL_VECTOR, NULL_VECTOR);
    
    // Normalize vector.
    NormalizeVector(viewvec, viewvec);
    
    // Scale to the given distance.
    ScaleVector(viewvec, distance);
    
    // Add the vectors together.
    AddVectors(vec, viewvec, result);
}

/**
 * Get the max/min value of a 3D box on any axis.
 * 
 * @param axis          The axis to check.
 * @param boundaries    The boundaries to check.
 * @param min           Return the min value instead.
 */
stock Float:AntiStickGetBoxMaxBoundary(axis, Float:boundaries[AntiStickBoxBound][3], bool:min = false)
{
    // Create 'outlier' with initial value of first boundary.
    new Float:outlier = boundaries[0][axis];
    
    // x = Boundary index. (Start at 1 because we initialized 'outlier' with the 0 index's value)
    new size = sizeof(boundaries);
    for (new x = 1; x < size; x++)
    {
        if (!min && boundaries[x][axis] > outlier)
        {
            outlier = boundaries[x][axis];
        }
        else if (min && boundaries[x][axis] < outlier)
        {
            outlier = boundaries[x][axis];
        }
    }
    
    // Return value.
    return outlier;
}

/**
 * Checks if a player is currently stuck within another player.
 *
 * @param client1   The first client index.
 * @param client2   The second client index.
 * @return          True if they are stuck together, false if not.
 */
stock bool:AntiStickIsModelBoxColliding(client1, client2)
{
    new Float:client1modelbox[AntiStickBoxBound][3];
    new Float:client2modelbox[AntiStickBoxBound][3];
    
    // Get model hull widths.
    new Float:hull_width1 = AntiStickGetModelHullWidth(client1);
    new Float:hull_width2 = AntiStickGetModelHullWidth(client2);
    
    // Build model boxes for each client.
    AntiStickBuildModelBox(client1, client1modelbox, hull_width1);
    AntiStickBuildModelBox(client2, client2modelbox, hull_width2);
    
    // Compare x values.
    new Float:max1x = AntiStickGetBoxMaxBoundary(0, client1modelbox);
    new Float:max2x = AntiStickGetBoxMaxBoundary(0, client2modelbox);
    new Float:min1x = AntiStickGetBoxMaxBoundary(0, client1modelbox, true);
    new Float:min2x = AntiStickGetBoxMaxBoundary(0, client2modelbox, true);
    
    if (max1x < min2x || min1x > max2x)
    {
        return false;
    }
    
    // Compare y values.
    new Float:max1y = AntiStickGetBoxMaxBoundary(1, client1modelbox);
    new Float:max2y = AntiStickGetBoxMaxBoundary(1, client2modelbox);
    new Float:min1y = AntiStickGetBoxMaxBoundary(1, client1modelbox, true);
    new Float:min2y = AntiStickGetBoxMaxBoundary(1, client2modelbox, true);
    
    if (max1y < min2y || min1y > max2y)
    {
        return false;
    }
    
    // Compare z values.
    new Float:max1z = AntiStickGetBoxMaxBoundary(2, client1modelbox);
    new Float:max2z = AntiStickGetBoxMaxBoundary(2, client2modelbox);
    new Float:min1z = AntiStickGetBoxMaxBoundary(2, client1modelbox, true);
    new Float:min2z = AntiStickGetBoxMaxBoundary(2, client2modelbox, true);
    
    if (max1z < min2z || min1z > max2z)
    {
        return false;
    }
    
    // They are intersecting.
    return true;
}

/**
 * Sets the collision group on a client.
 *
 * @param client            The client index.
 * @param collisiongroup    Collision group flag.
 */
AntiStickSetCollisionGroup(client, collisiongroup)
{
    SetEntProp(client, Prop_Data, "m_CollisionGroup", collisiongroup);
}

/**
 * Gets the collision group on a client.
 *
 * @param client    The client index.
 * @return          The collision group on the client.
 */
AntiStickGetCollisionGroup(client)
{
    return GetEntProp(client, Prop_Data, "m_CollisionGroup");
}

/**
 * Converts a collision group value into a name.
 *
 * @param collisiongroup    The collision group to convert.
 * @param buffer            Destination string buffer.
 * @param maxlen            Size of destination buffer.
 * @return                  Number of cells written.
 */
AntiStickCollisionGroupToString(collisiongroup, String:buffer[], maxlen)
{
    switch (collisiongroup)
    {
        case COLLISION_GROUP_NONE:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_NONE");
        }
        case COLLISION_GROUP_DEBRIS:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_DEBRIS");
        }
        case COLLISION_GROUP_DEBRIS_TRIGGER:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_DEBRIS_TRIGGER");
        }
        case COLLISION_GROUP_INTERACTIVE_DEBRIS:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_INTERACTIVE_DEBRIS");
        }
        case COLLISION_GROUP_INTERACTIVE:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_INTERACTIVE");
        }
        case COLLISION_GROUP_PLAYER:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_PLAYER");
        }
        case COLLISION_GROUP_BREAKABLE_GLASS:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_BREAKABLE_GLASS");
        }
        case COLLISION_GROUP_VEHICLE:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_VEHICLE");
        }
        case COLLISION_GROUP_PLAYER_MOVEMENT:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_PLAYER_MOVEMENT");
        }
        case COLLISION_GROUP_NPC:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_NPC");
        }
        case COLLISION_GROUP_IN_VEHICLE:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_IN_VEHICLE");
        }
        case COLLISION_GROUP_WEAPON:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_WEAPON");
        }
        case COLLISION_GROUP_VEHICLE_CLIP:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_VEHICLE_CLIP");
        }
        case COLLISION_GROUP_PROJECTILE:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_PROJECTILE");
        }
        case COLLISION_GROUP_DOOR_BLOCKER:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_DOOR_BLOCKER");
        }
        case COLLISION_GROUP_PASSABLE_DOOR:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_PASSABLE_DOOR");
        }
        case COLLISION_GROUP_DISSOLVING:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_DISSOLVING");
        }
        case COLLISION_GROUP_PUSHAWAY:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_PUSHAWAY");
        }
        case COLLISION_GROUP_NPC_ACTOR:
        {
            return strcopy(buffer, maxlen, "COLLISION_GROUP_NPC_ACTOR");
        }
    }
    
    // No match. Write a blank string.
    return strcopy(buffer, maxlen, "");
}

/**
 * Command callback (zr_antistick_list_models)
 * Lists all player's models and model hull data.
 * 
 * @param client    The client index.
 * @param argc      Argument count.
 */
public Action:AntiStickListModelsCommand(client, argc)
{
    // Tell client we are listing model data.
    TranslationPrintToConsole(client, "AntiStick command list models list");
    
    decl String:clientname[MAX_NAME_LENGTH];
    decl String:modelname[PLATFORM_MAX_PATH];
    new Float:hull_width;
    
    // x = Client index.
    for (new x = 1; x <= MaxClients; x++)
    {
        // If client isn't in-game, then stop.
        if (!IsClientInGame(x))
        {
            continue;
        }
        
        // Get all needed data.
        GetClientName(x, clientname, sizeof(clientname));
        GetClientModel(x, modelname, sizeof(modelname));
        hull_width = AntiStickGetModelHullWidth(x);
    
        TranslationPrintToConsole(client, "AntiStick command list models name", clientname, modelname, hull_width);
    }
    
    return Plugin_Handled;
}

/**
 * Command callback (zr_antistick_set_width)
 * Set the hull width on any model.
 * 
 * @param client    The client index.
 * @param argc      Argument count.
 */
public Action:AntiStickSetWidthCommand(client, argc)
{
    // If not enough arguments given, then stop.
    if (argc < 2)
    {
        TranslationReplyToCommand(client, "AntiStick command set width syntax");
        return Plugin_Handled;
    }
    
    // Get target model.
    decl String:model[PLATFORM_MAX_PATH];
    GetCmdArg(1, model, sizeof(model));
    
    // If model doesn't exist, then stop.
    if (!FileExists(model))
    {
        new target = FindTarget(client, model);
        if (target <= 0)
        {
            TranslationReplyToCommand(client, "AntiStick command set width invalid model", model);
            return Plugin_Handled;
        }
        else
        {
            // Get the target's model.
            GetClientModel(target, model, sizeof(model));
        }
    }
    
    // Get the given hull width..
    decl String:strHullwidth[PLATFORM_MAX_PATH];
    GetCmdArg(2, strHullwidth, sizeof(strHullwidth));
    
    new Float:hull_width = StringToFloat(strHullwidth);
    if (hull_width <= 0.0)
    {
        TranslationReplyToCommand(client, "AntiStick command set width invalid width", hull_width);
        return Plugin_Handled;
    }
    
    // Set hull width.
    AntiStickSetModelHullWidth(0, model, hull_width);
    
    // Tell client it was successful.
    TranslationReplyToCommand(client, "AntiStick command set width successful", model, hull_width);
    
    // Save data.
    AntiStickSaveData();
    
    return Plugin_Handled;
}

/**
 * Command callback (zr_antistick_dump_group)
 * Dumps collision group data.
 * 
 * @param client    The client index.
 * @param argc      Argument count.
 */
public Action:AntiStickDumpGroupCommand(client, argc)
{
    new collisiongroup;
    new target;
    decl String:groupname[64];
    decl String:arg[96];
    
    // Write header.
    ReplyToCommand(client, "Player:                             Collision group:\n--------------------------------------------------------------------------------");
    
    if (argc < 1)
    {
        // Dump collision groups on all players.
        
        // Loop through all alive players.
        for (target = 1; target <= MaxClients; target++)
        {
            // Validate client state.
            if (!IsClientConnected(target) || !IsClientInGame(target) || !IsPlayerAlive(target))
            {
                continue;
            }
            
            // Get collision group name.
            collisiongroup = AntiStickGetCollisionGroup(target);
            AntiStickCollisionGroupToString(collisiongroup, groupname, sizeof(groupname));
            
            // List player name and collision group.
            ReplyToCommand(client, "%-35N %s", target, groupname);
        }
    }
    else
    {
        // Get the target.
        GetCmdArg(1, arg, sizeof(arg));
        target = FindTarget(client, arg);
        
        // Validate target.
        if (ZRIsClientValid(target))
        {
            // Get collision group name.
            collisiongroup = AntiStickGetCollisionGroup(target);
            AntiStickCollisionGroupToString(collisiongroup, groupname, sizeof(groupname));
            
            // List player name and collision group.
            ReplyToCommand(client, "%-35N %s", target, groupname);
        }
    }
    
    return Plugin_Handled;
}