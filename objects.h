#ifndef	_objects_h_
#define	_objects_h_

enum
{
    Empty = 0,

    Slide_North,
    Slide_West,
    Slide_South,
    Slide_East,
    Slide_Random,
    Ice,
    IceWall_Northwest,
    IceWall_Northeast,
    IceWall_Southwest,
    IceWall_Southeast,
    Gravel,
    Dirt,
    Water,
    Fire,
    Bomb,
    Beartrap,
    Burglar,
    HintButton,

    Button_Blue,
    Button_Green,
    Button_Red,
    Button_Brown,
    Teleport,

    Wall,
    Wall_North,
    Wall_West,
    Wall_South,
    Wall_East,
    Wall_Southeast,
    HiddenWall_Perm,
    HiddenWall_Temp,
    BlueWall_Real,
    BlueWall_Fake,
    SwitchWall_Open,
    SwitchWall_Closed,
    PopupWall,

    CloneMachine,

    Door_Blue,
    Door_Green,
    Door_Red,
    Door_Yellow,
    Socket,
    Exit,

    ICChip,
    Key_Blue,
    Key_Green,
    Key_Red,
    Key_Yellow,
    Boots_Slide,
    Boots_Ice,
    Boots_Water,
    Boots_Fire,

    Count_Floors
};

enum
{
    Nobody = 0,

    Chip,

    Block,

    Tank,
    Ball,
    Glider,
    Fireball,
    Walker,
    Blob,
    Teeth,
    Bug,
    Paramecium,

    Count_Entities
};

#define	isslide(f)	((f) >= Slide_North && (f) <= Slide_Random)
#define	isice(f)	((f) >= Ice && (f) <= IceWall_Southeast)
#define	isdoor(f)	((f) >= Door_Blue && (f) <= Door_Yellow)
#define	iskey(f)	((f) >= Key_Blue && (f) <= Key_Yellow)
#define	isboots(f)	((f) >= Boots_Slide && (f) <= Boots_Fire)

#endif
