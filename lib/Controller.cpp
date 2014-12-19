#include "Controller.h"
#include "ControllerManager.h"
#include "Constants.h"
#include "Exceptions.h"
#include "ErrorStrings.h"

#include <windows.h>

#include <cstdlib>
#include <limits>

using namespace std;


static string getVKeyName ( uint32_t vkCode, uint32_t scanCode, bool isExtended )
{
    switch ( vkCode )
    {
#include "KeyboardVKeyNames.h"

        default:
            break;
    }

    if ( isExtended )
        scanCode |= 0x100;

    char name[4096];

    if ( GetKeyNameText ( scanCode << 16, name, sizeof ( name ) ) > 0 )
        return name;
    else
        return format ( "Key Code 0x%02X", vkCode );
}

static inline char getAxisSign ( uint8_t value )
{
    if ( AXIS_POSITIVE )
        return '+';

    if ( AXIS_NEGATIVE )
        return '-';

    return '0';
}

void Controller::keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown )
{
    // Ignore keyboard events for joystick (except ESC)
    if ( isJoystick() && vkCode != VK_ESCAPE )
        return;

    // Only use keyboard down events for mapping
    if ( !isDown && keyToMap != 0 )
        return;

    Owner *owner = this->owner;
    uint32_t key = 0;

    if ( vkCode != VK_ESCAPE )
    {
        doClearMapping ( keyToMap );

        const string name = getVKeyName ( vkCode, scanCode, isExtended );

        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( keyToMap & ( 1u << i ) )
            {
                keyboardMappings.codes[i] = vkCode;
                keyboardMappings.names[i] = name;
            }
            else if ( keyboardMappings.codes[i] == vkCode )
            {
                keyboardMappings.codes[i] = 0;
                keyboardMappings.names[i].clear();
            }
        }

        keyboardMappings.invalidate();
        key = keyToMap;

        LOG_CONTROLLER ( this, "Mapped key [0x%02X] %s to %08x ", vkCode, name, keyToMap );
    }

    if ( options & MAP_CONTINUOUSLY )
        joystickMapState.clear();
    else
        cancelMapping();

    ControllerManager::get().mappingsChanged ( this );

    if ( owner )
        owner->doneMapping ( this, key );
}

void Controller::joystickAxisEvent ( uint8_t axis, uint8_t value )
{
    const uint32_t keyMapped = joystickMappings.axes[axis][value];

    if ( keyToMap != 0 && !waitForNeutral
            && ! ( ( options & MAP_PRESERVE_DIRS ) && ( joystickMappings.axes[axis][AXIS_CENTERED] & MASK_DIRS ) ) )
    {
        const uint8_t activeValue = joystickMapState.axes[axis];

        // Done mapping if the axis returned to 0
        if ( value == AXIS_CENTERED && activeValue != AXIS_CENTERED )
        {
            doClearMapping ( keyToMap );

            joystickMappings.axes[axis][activeValue] = keyToMap;

            // Set bit mask for neutral value
            joystickMappings.axes[axis][AXIS_CENTERED] =
                ( joystickMappings.axes[axis][AXIS_POSITIVE] |
                  joystickMappings.axes[axis][AXIS_NEGATIVE] );

            joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped value=%c to %08x", getAxisSign ( activeValue ), keyToMap );

            Owner *owner = this->owner;
            const uint32_t key = keyToMap;

            if ( options & MAP_CONTINUOUSLY )
                joystickMapState.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue != AXIS_CENTERED )
            return;

        joystickMapState.axes[axis] = value;
        return;
    }

    const uint32_t mask = joystickMappings.axes[axis][AXIS_CENTERED];

    if ( !mask )
        return;

    state &= ~mask;

    if ( value != AXIS_CENTERED )
        state |= keyMapped;

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    LOG_CONTROLLER ( this, "value=%c", getAxisSign ( value ) );
}

void Controller::joystickHatEvent ( uint8_t hat, uint8_t value )
{
    const uint32_t keyMapped = joystickMappings.hats[hat][value];

    if ( keyToMap != 0 && !waitForNeutral && ( value % 2 == 0 ) // Only map up/down/left/right
            && ! ( ( options & MAP_PRESERVE_DIRS ) && ( joystickMappings.hats[hat][5] & MASK_DIRS ) ) )
    {
        const uint8_t activeValue = joystickMapState.hats[hat];

        // Done mapping if the axis returned to 0
        if ( value == 5 && activeValue != 5 )
        {
            doClearMapping ( keyToMap );

            joystickMappings.hats[hat][value] = keyToMap;

            // Set bit mask for neutral value
            joystickMappings.hats[hat][5] = ( joystickMappings.hats[hat][2] | joystickMappings.hats[hat][4] |
                                              joystickMappings.hats[hat][6] | joystickMappings.hats[hat][8] );

            // Set bit masks for diagonal values
            joystickMappings.hats[hat][1] = ( joystickMappings.hats[hat][2] | joystickMappings.hats[hat][4] );
            joystickMappings.hats[hat][3] = ( joystickMappings.hats[hat][2] | joystickMappings.hats[hat][6] );
            joystickMappings.hats[hat][7] = ( joystickMappings.hats[hat][8] | joystickMappings.hats[hat][4] );
            joystickMappings.hats[hat][9] = ( joystickMappings.hats[hat][8] | joystickMappings.hats[hat][6] );

            joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped value=%d to %08x", activeValue, keyToMap );

            Owner *owner = this->owner;
            const uint32_t key = keyToMap;

            if ( options & MAP_CONTINUOUSLY )
                joystickMapState.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue != 5 )
            return;

        joystickMapState.hats[hat] = value;
        return;
    }

    const uint32_t mask = joystickMappings.hats[hat][5];

    if ( !mask )
        return;

    state &= ~mask;

    if ( value != 5 )
        state |= keyMapped;

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    LOG_CONTROLLER ( this, "value=%d", value );
}

void Controller::joystickButtonEvent ( uint8_t button, bool isDown )
{
    const uint32_t keyMapped = joystickMappings.buttons[button];

    if ( keyToMap != 0 && !waitForNeutral
            && ! ( ( options & MAP_PRESERVE_DIRS ) && ( keyMapped & MASK_DIRS ) ) )
    {
        const bool isActive = ( joystickMapState.buttons & ( 1u << button ) );

        // Done mapping if the button was released
        if ( !isDown && isActive )
        {
            doClearMapping ( keyToMap );

            joystickMappings.buttons[button] = keyToMap;

            joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped button%d to %08x", button, keyToMap );

            Owner *owner = this->owner;
            const uint32_t key = keyToMap;

            if ( options & MAP_CONTINUOUSLY )
                joystickMapState.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick buttons
        if ( isActive )
            return;

        joystickMapState.buttons |= ( 1u << button );
        return;
    }

    if ( isDown )
        anyButton |= ( 1u << button );
    else
        anyButton &= ~ ( 1u << button );

    if ( keyMapped == 0 )
        return;

    if ( isDown )
        state |= keyMapped;
    else
        state &= ~keyMapped;

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    LOG_CONTROLLER ( this, "button=%d; isDown=%d", button, isDown );
}

static unordered_set<string> namesWithIndex;

static unordered_map<string, uint32_t> origNameCount;

static string nextName ( const string& name )
{
    if ( namesWithIndex.find ( name ) == namesWithIndex.end() )
        return name;

    uint32_t index = 2;

    while ( namesWithIndex.find ( format ( "%s (%d)", name, index ) ) != namesWithIndex.end() )
    {
        if ( index == numeric_limits<uint32_t>::max() )
            THROW_EXCEPTION ( ERROR_TOO_MANY_CONTROLLERS, "", name );

        ++index;
    }

    return format ( "%s (%d)", name, index );
}

bool Controller::isUniqueName() const { return ( origNameCount[name] == 1 ); }

Controller::Controller ( KeyboardEnum ) : name ( "Keyboard" )
{
    keyboardMappings.name = name;
    namesWithIndex.insert ( keyboardMappings.name );
    origNameCount[name] = 1;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New keyboard" );
}

Controller::Controller ( const string& name, void *joystick, uint32_t numAxes, uint32_t numHats, uint32_t numButtons )
    : name ( name )
    , joystick ( joystick )
    , numAxes ( min ( numAxes, MAX_NUM_AXES ) )
    , numHats ( min ( numHats, MAX_NUM_HATS ) )
    , numButtons ( min ( numButtons, MAX_NUM_BUTTONS ) )
{
    joystickMappings.name = nextName ( name );
    namesWithIndex.insert ( joystickMappings.name );

    auto it = origNameCount.find ( name );
    if ( it == origNameCount.end() )
        origNameCount[name] = 1;
    else
        ++it->second;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New joystick: %s; %u axe(s); %u hat(s); %u button(s)", name, numAxes, numHats, numButtons );
}

Controller::~Controller()
{
    LOG_CONTROLLER ( this, "Deleting controller" );

    namesWithIndex.erase ( getName() );

    uint32_t& count = origNameCount[name];

    if ( count > 1 )
        --count;
    else
        origNameCount.erase ( name );
}

string Controller::getMapping ( uint32_t key, const string& placeholder ) const
{
    if ( key == keyToMap && !placeholder.empty() )
        return placeholder;

    if ( isKeyboard() )
    {
        for ( uint8_t i = 0; i < 32; ++i )
            if ( key & ( 1u << i ) && keyboardMappings.codes[i] )
                return keyboardMappings.names[i];

        return "";
    }
    else if ( isJoystick() )
    {
        string mapping;

        for ( uint8_t axis = 0; axis < MAX_NUM_AXES; ++axis )
        {
            for ( uint8_t value = AXIS_POSITIVE; value <= AXIS_NEGATIVE; ++value )
            {
                if ( joystickMappings.axes[axis][value] != key )
                    continue;

                // char type;
                // uint8_t index;
                // splitAxis ( axis, type, index );

                // string str;
                // if ( index == 0 )
                //     str = format ( "%c %c-Axis", getAxisSign ( value ), type );
                // else
                //     str = format ( "%c %c-Axis (%u)", getAxisSign ( value ), type, index + 1 );

                // if ( mapping.empty() )
                //     mapping = str;
                // else
                //     mapping += ", " + str;
            }
        }

        // TODO hat

        for ( uint8_t button = 0; button < MAX_NUM_BUTTONS; ++button )
        {
            if ( joystickMappings.buttons[button] != key )
                continue;

            const string str = format ( "Button %u", button + 1 );

            if ( mapping.empty() )
                mapping = str;
            else
                mapping += ", " + str;
        }

        return mapping;
    }

    return "";
}

void Controller::setMappings ( const array<char, 10>& config )
{
    LOG_CONTROLLER ( this, "Raw keyboard mappings" );

    static const array<uint32_t, 10> bits =
    {
        BIT_DOWN,
        BIT_UP,
        BIT_LEFT,
        BIT_RIGHT,
        ( CC_BUTTON_A | CC_BUTTON_CONFIRM ) << 8,
        ( CC_BUTTON_B | CC_BUTTON_CANCEL ) << 8,
        CC_BUTTON_C << 8,
        CC_BUTTON_D << 8,
        CC_BUTTON_E << 8,
        CC_BUTTON_START << 8,
    };

    for ( uint8_t i = 0; i < 10; ++i )
    {
        const uint32_t vkCode = MapVirtualKey ( config[i], MAPVK_VSC_TO_VK_EX );
        const string name = getVKeyName ( vkCode, config[i], false );

        for ( uint8_t j = 0; j < 32; ++j )
        {
            if ( bits[i] & ( 1u << j ) )
            {
                keyboardMappings.codes[j] = vkCode;
                keyboardMappings.names[j] = name;
            }
            else if ( keyboardMappings.codes[j] == vkCode )
            {
                keyboardMappings.codes[j] = 0;
                keyboardMappings.names[j].clear();
            }
        }
    }

    keyboardMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const KeyboardMappings& mappings )
{
    LOG_CONTROLLER ( this, "KeyboardMappings" );

    keyboardMappings = mappings;
    keyboardMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const JoystickMappings& mappings )
{
    LOG_CONTROLLER ( this, "JoystickMappings" );

    joystickMappings = mappings;
    joystickMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::startMapping ( Owner *owner, uint32_t key, const void *window,
                                const unordered_set<uint32_t>& ignore, uint8_t options )
{
    if ( this->options & MAP_CONTINUOUSLY )
        joystickMapState.clear();
    else
        cancelMapping();

    LOG_CONTROLLER ( this, "Starting mapping %08x", key );

    if ( state )
        waitForNeutral = true;

    this->owner = owner;
    this->keyToMap = key;
    this->options = options;

    if ( isKeyboard() )
        KeyboardManager::get().matchedKeys.clear(); // Check all except ignored keys
    else
        KeyboardManager::get().matchedKeys = { VK_ESCAPE }; // Only check ESC key

    KeyboardManager::get().ignoredKeys = ignore;
    KeyboardManager::get().hook ( this );
}

void Controller::cancelMapping()
{
    LOG_CONTROLLER ( this, "Cancel mapping %08x", keyToMap );

    KeyboardManager::get().unhook();

    owner = 0;
    keyToMap = 0;
    waitForNeutral = false;

    joystickMapState.clear();
}

void Controller::doClearMapping ( uint32_t keys )
{
    for ( uint8_t i = 0; i < 32; ++i )
    {
        if ( keys & ( 1u << i ) )
        {
            keyboardMappings.codes[i] = 0;
            keyboardMappings.names[i].clear();
            keyboardMappings.invalidate();
        }
    }

    for ( auto& a : joystickMappings.axes )
    {
        for ( auto& b : a )
        {
            if ( b & keys )
            {
                b = 0;
                joystickMappings.invalidate();
            }
        }
    }

    for ( auto& a : joystickMappings.hats )
    {
        for ( auto& b : a )
        {
            if ( b & keys )
            {
                b = 0;
                joystickMappings.invalidate();
            }
        }
    }

    for ( auto& a : joystickMappings.buttons )
    {
        if ( a & keys )
        {
            a = 0;
            joystickMappings.invalidate();
        }
    }
}

void Controller::clearMapping ( uint32_t keys )
{
    doClearMapping ( keys );

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::doResetToDefaults()
{
    if ( !isJoystick() )
        return;

    // Clear all buttons
    doClearMapping ( MASK_BUTTONS );

    // // Default axis mappings
    // for ( uint8_t i = 0; i < 3; ++i )
    // {
    //     joystickMappings.axes[ combineAxis ( 'X', i ) ][AXIS_CENTERED] = MASK_X_AXIS;
    //     joystickMappings.axes[ combineAxis ( 'X', i ) ][AXIS_POSITIVE] = BIT_RIGHT;
    //     joystickMappings.axes[ combineAxis ( 'X', i ) ][AXIS_NEGATIVE] = BIT_LEFT;
    //     joystickMappings.axes[ combineAxis ( 'Y', i ) ][AXIS_CENTERED] = MASK_Y_AXIS;
    //     joystickMappings.axes[ combineAxis ( 'Y', i ) ][AXIS_POSITIVE] = BIT_DOWN;
    //     joystickMappings.axes[ combineAxis ( 'Y', i ) ][AXIS_NEGATIVE] = BIT_UP;
    // }

    // Default deadzone
    joystickMappings.deadzone = DEFAULT_DEADZONE;

    joystickMappings.invalidate();
}

void Controller::resetToDefaults()
{
    if ( !isJoystick() )
        return;

    doResetToDefaults();

    ControllerManager::get().mappingsChanged ( this );
}

bool Controller::saveMappings ( const string& file ) const
{
    if ( isKeyboard() )
        return ControllerManager::saveMappings ( file, keyboardMappings );
    else
        return ControllerManager::saveMappings ( file, joystickMappings );
}

bool Controller::loadMappings ( const string& file )
{
    MsgPtr msg = ControllerManager::loadMappings ( file );

    if ( !msg )
        return false;

    if ( isKeyboard() )
    {
        if ( msg->getMsgType() != MsgType::KeyboardMappings )
        {
            LOG ( "Invalid keyboard mapping type: %s", msg->getMsgType() );
            return false;
        }

        if ( msg->getAs<KeyboardMappings>().name != keyboardMappings.name )
        {
            LOG ( "Name mismatch: decoded '%s' != keyboard '%s'",
                  msg->getAs<KeyboardMappings>().name, keyboardMappings.name );
        }

        keyboardMappings = msg->getAs<KeyboardMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }
    else // if ( isJoystick() )
    {
        if ( msg->getMsgType() != MsgType::JoystickMappings )
        {
            LOG ( "Invalid joystick mapping type: %s", msg->getMsgType() );
            return false;
        }

        if ( msg->getAs<JoystickMappings>().name != joystickMappings.name )
        {
            LOG ( "Name mismatch: decoded '%s' != joystick '%s'",
                  msg->getAs<JoystickMappings>().name, joystickMappings.name );
        }

        joystickMappings = msg->getAs<JoystickMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }

    return true;
}
