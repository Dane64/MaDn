#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "inputHandler.h"
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>
#include <vitasdk.h>

#define WIDTH 960 // Screen width
#define HEIGHT 544 // Scree height
#define POFF 10 // Player number offset (Ex: POFF == 10, P1 = 10, P2 = 20)

#define RED     RGBA8(255,   0,   0, 255)
#define YELLOW  RGBA8(255, 255,   0, 255)
#define BLUE    RGBA8(  0,   0, 255, 255)
#define WHITE   RGBA8(255, 255, 255, 255)
#define BLACK   RGBA8(  0,   0,   0, 255)
#define GREEN   RGBA8(  0, 255,   0, 255)
#define BROWN   RGBA8(139,  69,  19, 255)
#define WHEAT   RGBA8(245, 222, 179, 255)
#define PINK    RGBA8(255, 192, 203, 255)
#define ALMOND	RGBA8(209, 182, 137, 255)

typedef enum tEnumPlayer{
    NoPosition,
	Empty,
	PlayerOne = 1*POFF,
    PlayerOneHome,
    PlayerOneStart,
	PlayerTwo = 2*POFF,
	PlayerTwoHome,
    PlayerTwoStart,
    PlayerThree = 3*POFF,
    PlayerThreeHome,
    PlayerThreeStart,
    PlayerFour = 4*POFF,
    PlayerFourHome,
    PlayerFourStart
} tEnumPlayer;

typedef struct tStBoard
{
	unsigned short uiX;
	unsigned short uiY;
	tEnumPlayer eData;
} tStBoard;

typedef struct tStGame
{
	tStBoard **Field;
	tEnumPlayer eTurn;
	unsigned short uiCellWidth;
	unsigned short uiCellHeight;
	unsigned short uiFieldWidth;
	unsigned short uiFieldHeight;
} tStGame;

typedef struct tStPosition
{
    unsigned short uiRowIndex;
    unsigned short uiColIndex;
    unsigned short uiMovesLeft;
} tStPosition;

typedef enum tEnumGameState
{
    Waiting,
    ThrowingDice,
    ThrewHighestPips,
    SummoningPawn,
    PickingPawn,
    MovingPawn,
    AnimatingPawn,
    EndingTurn
} tEnumGameState;

void BoardConstructor(tStGame *stGame)
{
	stGame->uiCellHeight = HEIGHT / stGame->uiFieldHeight;
	stGame->uiCellWidth = stGame->uiCellHeight;

    unsigned short uiIncreaseRow = 0;
	stGame->Field = malloc(sizeof(tStBoard *)*stGame->uiFieldHeight);

    for (int i=0; i<stGame->uiFieldHeight; i++){
        uiIncreaseRow += i == 0 ? stGame->uiCellHeight/2 : stGame->uiCellHeight;
        stGame->Field[i] = malloc(sizeof(tStBoard)*stGame->uiFieldWidth);
        for (int j=0; j<stGame->uiFieldWidth; j++){
            stGame->Field[i][j].eData = NoPosition;
            stGame->Field[i][j].uiY = uiIncreaseRow;
            stGame->Field[i][j].uiX = j == 0 ? (WIDTH-(stGame->uiCellWidth*stGame->uiFieldWidth))/2 : stGame->Field[i][j-1].uiX + stGame->uiCellWidth;
        }
    }
}

void BoardDestructor(tStGame *stGame)
{
    for (int i=0; i<stGame->uiFieldHeight; i++){
        free(stGame->Field[i]);
    }
    free(stGame->Field);
}

void CreatePlayers(tStGame *stGame)
{
    for (int i=0; i<stGame->uiFieldHeight; i++){
        for (int j=0; j<stGame->uiFieldWidth; j++){
            if (i%(stGame->uiFieldHeight-2) < 2 && j%(stGame->uiFieldWidth-2) < 2){
                if (i < 2 && j < 2){
                   stGame->Field[i][j].eData = PlayerOne;
                } else if (i > 8 && j < 2){
                    stGame->Field[i][j].eData = PlayerTwo;
                } else if (i < 2 && j > 8){
                    stGame->Field[i][j].eData = PlayerThree;
                } else if (i > 8 && j > 8){
                    stGame->Field[i][j].eData = PlayerFour;
                }
            }
        }
    }
}

void CreatePlayingCircle(tStGame *stGame)
{
    for (int i=0; i<stGame->uiFieldHeight; i++){
        for (int j=0; j<stGame->uiFieldWidth; j++){
            if ((i>3 && i<7) || (j>3 && j<7)){  
                stGame->Field[i][j].eData = Empty;
            }
        }
    }
}

void CreateHomePositions(tStGame *stGame)
{
    unsigned short uiI = stGame->uiFieldWidth / 2;
    unsigned short uiJ = stGame->uiFieldHeight / 2;
    stGame->Field[uiI][uiJ].eData = NoPosition;

    for (int i=0; i<4; i++){ // Amount of players
        for (int j=1; j<5; j++){ // Amount of home positions
            if (i == 0){
                stGame->Field[uiI][uiJ-j].eData = PlayerOneHome;
            } else if (i == 1){
                stGame->Field[uiI+j][uiJ].eData = PlayerTwoHome;
            } else if (i == 2){
                stGame->Field[uiI-j][uiJ].eData = PlayerThreeHome;
            } else if (i == 3){
                stGame->Field[uiI][uiJ+j].eData = PlayerFourHome;
            }
        }
    }  
}

void CreateStartPositions(tStGame *stGame)
{
    stGame->Field[stGame->uiFieldHeight/2-1][0].eData = PlayerOneStart;
    stGame->Field[stGame->uiFieldHeight-1][stGame->uiFieldWidth/2-1].eData = PlayerTwoStart;
    stGame->Field[0][stGame->uiFieldWidth/2+1].eData = PlayerThreeStart;
    stGame->Field[stGame->uiFieldHeight/2+1][stGame->uiFieldWidth-1].eData = PlayerFourStart;
}

void BoardInitializer(tStGame *stGame)
{
    CreatePlayers(stGame);
    CreatePlayingCircle(stGame);
    CreateHomePositions(stGame);
    // CreateStartPositions(stGame);
}

unsigned short RollDice()
{
    return (rand() % 6) + 1;
}

tStPosition ChoosePawn(tStGame *stGame, unsigned short i, unsigned short j)
{
    tStPosition stPos = {-1, -1, 0};

    if (stGame->Field[i][j].eData == stGame->eTurn){ // Did user selected his own pawn ?
        if ((i>3 && i<7) || (j>3 && j<7)){ // Is the pawn on the field ?
            if (j != stGame->uiFieldWidth/2 && i != stGame->uiFieldHeight/2 ){ // Is the pawn not in the home position ?
                stPos.uiRowIndex = i; stPos.uiColIndex = j;
            } else if ((i == stGame->uiFieldHeight/2 && (j == 0 || j == stGame->uiFieldWidth-1)) || (j == stGame->uiFieldWidth/2 && (i == 0 || i == stGame->uiFieldHeight-1))){
                stPos.uiRowIndex = i; stPos.uiColIndex = j;
            }
        }
    }

    return stPos;
}

tStPosition MovePawn(tStGame *stGame, unsigned short i, unsigned short j, unsigned short uiMoves)
{
    //             CLOCKWISE                              ANTI CLOCKWISE
	// j0 j1 j2 j3 j4 j5 j6 j7 j8 j9 j10        j0 j1 j2 j3 j4 j5 j6 j7 j8 j9 j10
	// [1][1][ ][ ][→][→][↓][ ][ ][3][3] i0     [1][1][ ][ ][↓][←][←][ ][ ][3][3] i0
	// [1][1][ ][ ][↑][|][↓][ ][ ][3][3] i1     [1][1][ ][ ][↓][|][↑][ ][ ][3][3] i1
	// [ ][ ][ ][ ][↑][|][↓][ ][ ][ ][ ] i2     [ ][ ][ ][ ][↓][|][↑][ ][ ][ ][ ] i2
	// [ ][ ][ ][ ][↑][|][↓][ ][ ][ ][ ] i3     [ ][ ][ ][ ][↓][|][↑][ ][ ][ ][ ] i3
	// [→][→][→][→][↑][|][→][→][→][→][↓] i4     [↓][←][←][←][←][|][↑][←][←][←][←] i4
	// [↑][—][—][—][—][⚄][—][—][—][—][↓] i5     [↓][—][—][—][—][⚄][—][—][—][—][↑] i5
	// [↑][←][←][←][←][|][↓][←][←][←][←] i6     [→][→][→][→][→][|][→][→][→][→][↑] i6
	// [ ][ ][ ][ ][↑][|][↓][ ][ ][ ][ ] i7     [ ][ ][ ][ ][↓][|][↑][ ][ ][ ][ ] i7
	// [ ][ ][ ][ ][↑][|][↓][ ][ ][ ][ ] i8     [ ][ ][ ][ ][↓][|][↑][ ][ ][ ][ ] i8
	// [2][2][ ][ ][↑][|][↓][ ][ ][4][4] i9     [2][2][ ][ ][↓][|][↑][ ][ ][4][4] i9
	// [2][2][ ][ ][↑][←][←][ ][ ][4][4] i10    [2][2][ ][ ][→][→][↑][ ][ ][4][4] i10

    bool xHomePos = false;
    bool xPawnOnEdge = true;

    if (stGame->eTurn == PlayerOne && i == stGame->uiFieldHeight/2 && j == 0){
        xHomePos = true;
    } else if (stGame->eTurn == PlayerTwo && i == stGame->uiFieldHeight-1 && j == stGame->uiFieldWidth/2){
        xHomePos = true;
    } else if (stGame->eTurn == PlayerThree && i == 0 && j == stGame->uiFieldWidth/2){
        xHomePos = true;
    } else if (stGame->eTurn == PlayerFour && i == stGame->uiFieldHeight/2 && j == stGame->uiFieldWidth-1){
        xHomePos = true;
    }
    
    if (uiMoves == 0 || xHomePos){
        tStPosition stPos = {i, j, uiMoves};
        return stPos;
    }

    if (i == stGame->uiFieldHeight/2-1 && j != stGame->uiFieldWidth/2-1 && j != stGame->uiFieldWidth-1){
        xPawnOnEdge = false;
        j++;
    } else if (i == stGame->uiFieldHeight/2+1 && j != stGame->uiFieldWidth/2+1 && j != 0){
        xPawnOnEdge = false;
        j--;
    } else if (j == stGame->uiFieldWidth/2-1 && i != stGame->uiFieldHeight/2+1 && i != 0){
        xPawnOnEdge = false;
        i--;
    } else if (j == stGame->uiFieldWidth/2+1 && i != stGame->uiFieldHeight/2-1 && i != stGame->uiFieldHeight-1){
        xPawnOnEdge = false;
        i++;
    }

    if (xPawnOnEdge){
        if (i == 0){
            j++;
        } else if (i == stGame->uiFieldHeight-1){
            j--;
        } else if (j == 0){
            i--;
        } else if (j == stGame->uiFieldWidth-1){
            i++;
        }
    }

    uiMoves--;
    return MovePawn(stGame, i, j, uiMoves);
}

tStPosition CheckStartPos(tStGame *stGame, tEnumPlayer ePlayer, bool xFindEmptySpot)
{
    tStPosition stPos = {-1, -1, 0};

    if (ePlayer == PlayerOne){
        for (int i=0; i<2; i++){
            for (int j=0; j<2; j++){
                if (xFindEmptySpot && stGame->Field[i][j].eData == Empty){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                } else if (!xFindEmptySpot && stGame->Field[i][j].eData == PlayerOne){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                }
            }
        }
    } else if (ePlayer == PlayerTwo){
        for (int i=stGame->uiFieldHeight-2; i<stGame->uiFieldHeight; i++){
            for (int j=0; j<2; j++){
                if (xFindEmptySpot && stGame->Field[i][j].eData == Empty){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                } else if (!xFindEmptySpot && stGame->Field[i][j].eData == PlayerTwo){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                }
            }
        }
    } else if (ePlayer == PlayerThree){
        for (int i=0; i<2; i++){
            for (int j=stGame->uiFieldWidth-2; j<stGame->uiFieldWidth; j++){
                if (xFindEmptySpot && stGame->Field[i][j].eData == Empty){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                } else if (!xFindEmptySpot && stGame->Field[i][j].eData == PlayerThree){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                }
            }
        }
    }else if (ePlayer == PlayerFour){
        for (int i=stGame->uiFieldHeight-2; i<stGame->uiFieldHeight; i++){
            for (int j=stGame->uiFieldWidth-2; j<stGame->uiFieldWidth; j++){
                if (xFindEmptySpot && stGame->Field[i][j].eData == Empty){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                } else if (!xFindEmptySpot && stGame->Field[i][j].eData == PlayerFour){
                    stPos.uiRowIndex = i; stPos.uiColIndex = j;
                }
            }
        }
    }

    return stPos;
}

void RemovePlayer(tStGame *stGame, tStPosition stNewPos)
{
    tStPosition stPos = CheckStartPos(stGame, stGame->Field[stNewPos.uiRowIndex][stNewPos.uiColIndex].eData, true);
    stGame->Field[stPos.uiRowIndex][stPos.uiColIndex].eData = stGame->Field[stNewPos.uiRowIndex][stNewPos.uiColIndex].eData;
}

tStPosition CheckHit(tStGame *stGame, tStPosition stNewPos, tStPosition stOldPos)
{
    tStPosition stPos = stNewPos;

    if (stGame->Field[stNewPos.uiRowIndex][stNewPos.uiColIndex].eData % POFF == 0){ // Modulus of POFF means it hit one of the four players
        RemovePlayer(stGame, stNewPos); // Place the hitted player back in the starting pos
    }

    // stGame->Field[stNewPos.uiRowIndex][stNewPos.uiColIndex].eData = stGame->eTurn; // Set current player to the new pos
    stGame->Field[stOldPos.uiRowIndex][stOldPos.uiColIndex].eData = Empty; // Remove old traces of the current player

    return stPos;
}

void SwitchPlayer(tStGame *stGame)
{
    stGame->eTurn+= POFF;

    if (stGame->eTurn > PlayerFour){
        stGame->eTurn = POFF;
    }
}

tStPosition SummonPawn(tStGame *stGame)
{
    tStPosition stPos = {-1, -1, 0};

    if (stGame->eTurn == PlayerOne){
        stPos.uiRowIndex = stGame->uiFieldHeight/2-1;
        stPos.uiColIndex = 0;
    } else if (stGame->eTurn == PlayerTwo){
        stPos.uiRowIndex = stGame->uiFieldHeight-1;
        stPos.uiColIndex = stGame->uiFieldWidth/2-1;
    } else if (stGame->eTurn == PlayerThree){
        stPos.uiRowIndex = 0;
        stPos.uiColIndex = stGame->uiFieldWidth/2+1;
    } else if (stGame->eTurn == PlayerFour){
        stPos.uiRowIndex = stGame->uiFieldHeight/2+1;
        stPos.uiColIndex = stGame->uiFieldWidth-1;
    }

    return stPos;
}

unsigned short GetNumberOfSummonedPawns(tStGame *stGame)
{
    unsigned short uiCount = 0;

    for (int i=0; i<stGame->uiFieldHeight; i++){
        for (int j=0; j<stGame->uiFieldWidth; j++){
            if (((i>3 && i<7) || (j>3 && j<7)) && stGame->Field[i][j].eData == stGame->eTurn){
                if (j != stGame->uiFieldWidth/2 && i != stGame->uiFieldHeight/2){ // Exclude (home positions) out of check
                    uiCount++;
                } else if ((i == stGame->uiFieldHeight/2 && (j == 0 || j == stGame->uiFieldWidth-1)) || (j == stGame->uiFieldWidth/2 && (i == 0 || i == stGame->uiFieldHeight-1))){ // The edges of the excluded area hold valid pawn locations
                    uiCount++;
                }
            }
        }
    }

    return uiCount;
}

tStPosition SetPlayerInHome(tStGame *stGame, tStPosition stNewPos)
{
    tStPosition stPos = stNewPos;
    unsigned short uiMoves = stNewPos.uiMovesLeft > 4 ? stNewPos.uiMovesLeft - (stNewPos.uiMovesLeft%4)*2 : stNewPos.uiMovesLeft;

    if (stGame->eTurn == PlayerOne){
        if (stGame->Field[stNewPos.uiRowIndex][stNewPos.uiColIndex+uiMoves].eData == PlayerOneHome){
            stPos.uiColIndex = stNewPos.uiColIndex+uiMoves; stPos.uiMovesLeft = 0;
        }
    } else if (stGame->eTurn == PlayerTwo){
        if (stGame->Field[stNewPos.uiRowIndex-uiMoves][stNewPos.uiColIndex].eData == PlayerTwoHome){
            stPos.uiRowIndex = stNewPos.uiRowIndex-uiMoves; stPos.uiMovesLeft = 0;
        }
    } else if (stGame->eTurn == PlayerThree){
        if (stGame->Field[stNewPos.uiRowIndex+uiMoves][stNewPos.uiColIndex].eData == PlayerThreeHome){
            stPos.uiRowIndex = stNewPos.uiRowIndex+uiMoves; stPos.uiMovesLeft = 0;
        }
    } else if (stGame->eTurn == PlayerFour){
        if (stGame->Field[stNewPos.uiRowIndex][stNewPos.uiColIndex-uiMoves].eData == PlayerFourHome){
            stPos.uiColIndex = stNewPos.uiColIndex-uiMoves; stPos.uiMovesLeft = 0;
        }
    }

    return stPos;
}

bool CheckWinner(tStGame *stGame)
{
    unsigned short uiI = stGame->uiFieldWidth / 2;
    unsigned short uiJ = stGame->uiFieldHeight / 2;

    for (int j=1; j<5; j++){ // Amount of home positions
        if (stGame->eTurn == PlayerOne && stGame->Field[uiI][uiJ-j].eData != PlayerOne){
            return false;
        } else if (stGame->eTurn == PlayerTwo && stGame->Field[uiI+j][uiJ].eData != PlayerTwo){
            return false;
        } else if (stGame->eTurn == PlayerThree && stGame->Field[uiI-j][uiJ].eData != PlayerThree){
            return false;
        } else if (stGame->eTurn == PlayerFour && stGame->Field[uiI][uiJ+j].eData != PlayerFour){
            return false;
        }
    }

    return true;
}

unsigned short GetDistToHomePos(tStGame *stGame, tStPosition stOldPos)
{
    tStPosition stNewPos = MovePawn(stGame, stOldPos.uiRowIndex, stOldPos.uiColIndex, 8*stGame->uiFieldWidth / 2); // 8 * 5 is maximum amount of steps possible
    return 8*stGame->uiFieldWidth / 2 - stNewPos.uiMovesLeft;
}

tStPosition PickPawnComputer(tStGame *stGame, unsigned short uiDice)
{
    tStPosition astPos[4];
    tStPosition stBestPos = {-1, -1, 0};
    unsigned short n = 0;
    unsigned short uiDist = 9999;

    // Get all the pawns that are on the board
    for (int i=0; i<stGame->uiFieldHeight; i++){
        for (int j=0; j<stGame->uiFieldWidth; j++){
            if (((i>3 && i<7) || (j>3 && j<7)) && stGame->Field[i][j].eData == stGame->eTurn){
                if (j != stGame->uiFieldWidth/2 && i != stGame->uiFieldHeight/2){ // Exclude (home positions) out of check
                    astPos[n].uiRowIndex = i; astPos[n].uiColIndex = j;
                    n++;
                } else if ((i == stGame->uiFieldHeight/2 && (j == 0 || j == stGame->uiFieldWidth-1)) || (j == stGame->uiFieldWidth/2 && (i == 0 || i == stGame->uiFieldHeight-1))){ // The edges of the excluded area hold valid pawn locations
                    astPos[n].uiRowIndex = i; astPos[n].uiColIndex = j;
                    n++;
                }
            }
        }
    }

    // Find out what pawn has to walk the shortest distance to the home pos
    unsigned short k[4] = {99, 99, 99, 99};
    for (int i=0; i<GetNumberOfSummonedPawns(stGame); i++){
        k[i] = GetDistToHomePos(stGame, astPos[i]);
        if(k[i] < uiDist){
            uiDist = k[i];
            stBestPos.uiRowIndex = astPos[i].uiRowIndex; stBestPos.uiColIndex = astPos[i].uiColIndex;
        }
    }

    // If computer is able to hit pawn which is not his own, hit that pawn OR If pawn cannot enter the home pos because of incorrect pips choose other pawn to move
    for (int i=0; i<GetNumberOfSummonedPawns(stGame); i++){
        tStPosition stTemp = MovePawn(stGame, astPos[i].uiRowIndex, astPos[i].uiColIndex, uiDice);
        if(stGame->Field[stTemp.uiRowIndex][stTemp.uiColIndex].eData % POFF == 0 && stTemp.uiMovesLeft == 0){ // Modulus of POFF means it hit one of the four players){
            if(stGame->Field[stTemp.uiRowIndex][stTemp.uiColIndex].eData != stGame->eTurn){
                stBestPos.uiRowIndex = astPos[i].uiRowIndex; stBestPos.uiColIndex = astPos[i].uiColIndex;
            }
        } else{
            tStPosition stTemp2 = SetPlayerInHome(stGame, stTemp);
            if (stTemp2.uiMovesLeft != 0){ // Pawn cannot enter home pos find second closest pawn to home pos
                unsigned short uiTemp1 = 9999;
                unsigned short uiTemp2 = 9999;
                for (int i=0; i<GetNumberOfSummonedPawns(stGame); i++){
                    if(k[i] <= uiTemp1){
                        uiTemp2 = uiTemp1;
                        uiTemp1 = k[i];
                    } else if(k[i] <= uiTemp2){
                        uiTemp2 = k[i];
                    }
                }
                for (int i=0; i<GetNumberOfSummonedPawns(stGame); i++){
                    if(k[i] == uiTemp2){
                        stBestPos.uiRowIndex = astPos[i].uiRowIndex; stBestPos.uiColIndex = astPos[i].uiColIndex;
                    }
                }
            }
        }
    }

    return stBestPos;
}

int main(void)
{
	vita2d_init();
    startInput();
    bool xAniDone = true;
    bool xAniInit = false;
    unsigned short uiDice = 0;
    unsigned short uiI = 0;
    unsigned short uiJ = 0;
    unsigned short uiNrOfMaxPips = 0;
    tEnumGameState eGameplayState = Waiting;

    SceDateTime Time;
    tStGame stGame;
    stGamePad stMcd;
    tStPosition stPos = {-1, -1, 0};
    tStPosition stAniPos = {-1, -1, 0};   
    stGame.eTurn = PlayerOne;
    sceRtcGetCurrentClockLocalTime(&Time);
    srand(sceRtcGetMicrosecond(&Time));

    stGame.uiFieldHeight = 11;
    stGame.uiFieldWidth = 11;
    BoardConstructor(&stGame);
    BoardInitializer(&stGame);

	while(!stMcd.stButt[6].xTrigger)
	{
		vita2d_start_drawing();
		vita2d_clear_screen();

        inputRead(&stMcd);

        if (stMcd.stDpad[0].xTrigger)
        {
            if (stMcd.stDpad[0].xLeft || stMcd.stDpad[0].xRight)
            {
                stMcd.stDpad[0].xLeft == 0 ? uiJ++ : uiJ--;
                uiJ = limit(stGame.uiFieldWidth-1, 0, uiJ);
            }
            else
            {
                stMcd.stDpad[0].xUp == 0 ? uiI++ : uiI--;
                uiI = limit(stGame.uiFieldHeight-1, 0, uiI);
            }
        }

        if (stMcd.stTouch[0].xTrigger)
        {
            float rDist = 0;
            float rClosest = WIDTH;

            for (int i=0; i<stGame.uiFieldHeight; i++){
                for (int j=0; j<stGame.uiFieldWidth; j++){
                    rDist = sqrtf(powf(stGame.Field[i][j].uiX - stMcd.stTouch[0].uiX, 2) + powf(stGame.Field[i][j].uiY - stMcd.stTouch[0].uiY, 2));
                    if (rDist < rClosest){
                        rClosest = rDist;
                        uiI = i; uiJ = j;
                    }
                }
            }
        } 

        tStPosition stOldPos;
        tStPosition stNewPos;

        if (!CheckWinner(&stGame))
        {
            switch (eGameplayState){

            case Waiting:
                uiDice = 0;
                if (stMcd.stButt[1].xTrigger || stMcd.stTouch[0].xTrigger || stGame.eTurn != PlayerOne){
                    eGameplayState = ThrowingDice;
                }
                break;

            case ThrowingDice:
                eGameplayState = PickingPawn;
                uiDice = RollDice();
                stNewPos = SummonPawn(&stGame); // Was there already an pawn summoned ?

                if (uiDice == 6 && uiNrOfMaxPips%2 == 0){ // Player threw 6
                    eGameplayState = ThrewHighestPips; // Check if player is alllowed to move pawn or summon new one
                } else if (uiNrOfMaxPips%2 == 1 && stGame.Field[stNewPos.uiRowIndex][stNewPos.uiColIndex].eData == stGame.eTurn){ // Player already summoned new pawn
                    eGameplayState = MovingPawn; // Force player to move the summoned pawn
                    uiI = stNewPos.uiRowIndex; // Set row index to summoned pawn location
                    uiJ = stNewPos.uiColIndex; // Set col index to summoned pawn location
                }
                
                break;

            case ThrewHighestPips:
                eGameplayState = PickingPawn;
                stOldPos = CheckStartPos(&stGame, stGame.eTurn, false);

                if (stOldPos.uiColIndex <= stGame.uiFieldWidth){
                    eGameplayState = SummoningPawn;
                }
                break;

            case SummoningPawn:
                eGameplayState = AnimatingPawn;
                xAniInit = true;
                stOldPos = CheckStartPos(&stGame, stGame.eTurn, false);
                stNewPos = SummonPawn(&stGame);

                stPos = CheckHit(&stGame, stNewPos, stOldPos);
                uiNrOfMaxPips++;
                break;

            case PickingPawn:
                if (stGame.eTurn == PlayerOne){
                    stOldPos = ChoosePawn(&stGame, uiI, uiJ);

                    if ((stMcd.stButt[1].xTrigger || stMcd.stTouch[0].xTrigger) && stOldPos.uiColIndex > stGame.uiFieldWidth){
                        vita2d_draw_rectangle(0, 0, WIDTH, HEIGHT, RED);
                    } else if (stMcd.stButt[1].xTrigger || stMcd.stTouch[0].xTrigger){
                        eGameplayState = MovingPawn;
                    }

                } else{
                    eGameplayState = MovingPawn;
                    stOldPos = PickPawnComputer(&stGame, uiDice);
                    if (stOldPos.uiColIndex <= stGame.uiFieldWidth){
                        uiI = stOldPos.uiRowIndex;
                        uiJ = stOldPos.uiColIndex;
                    }
                }
                
                if (GetNumberOfSummonedPawns(&stGame) == 0){
                    eGameplayState = EndingTurn;
                }
                
                break;

            case MovingPawn:
                eGameplayState = AnimatingPawn;
                uiNrOfMaxPips++;
                xAniInit = true;

                stOldPos = ChoosePawn(&stGame, uiI, uiJ);
                stNewPos = MovePawn(&stGame, stOldPos.uiRowIndex, stOldPos.uiColIndex, uiDice);

                if (stNewPos.uiMovesLeft == 0){
                    stPos = CheckHit(&stGame, stNewPos, stOldPos);
                } else{
                    stPos = SetPlayerInHome(&stGame, stNewPos);
                    if (stPos.uiMovesLeft == 0){
                        stGame.Field[stOldPos.uiRowIndex][stOldPos.uiColIndex].eData = Empty; // Remove old traces of the current player
                    } else{
                        stPos = stOldPos; // Move is impossible do not move player
                    }
                }

                break;

            case AnimatingPawn:
                xAniInit = false;

                if (xAniDone){
                    eGameplayState = uiDice == 6 ? Waiting : EndingTurn;
                    stPos.uiColIndex = -1;
                    stPos.uiRowIndex = -1;
                }
                break;

            case EndingTurn:
                eGameplayState = Waiting;
                uiNrOfMaxPips = 0;
                SwitchPlayer(&stGame);
                break;

            default :
                break;
            }
        } else{
            if(stMcd.stButt[2].xTrigger){
                BoardDestructor(&stGame);
                BoardConstructor(&stGame);
                BoardInitializer(&stGame);
                sceRtcGetCurrentClockLocalTime(&Time);
                srand(sceRtcGetMicrosecond(&Time));
                eGameplayState = Waiting;
            }
        }

        // Draw background
        vita2d_draw_rectangle(stGame.Field[0][0].uiX-stGame.uiCellWidth/2, stGame.Field[0][0].uiY-stGame.uiCellHeight/2, stGame.uiCellWidth*stGame.uiFieldWidth, stGame.uiCellHeight*stGame.uiFieldHeight, ALMOND);

        // Draw current 'mouse' position
        if (stGame.eTurn == PlayerOne){
            vita2d_draw_rectangle(stGame.Field[uiI][uiJ].uiX-stGame.uiCellWidth/2, stGame.Field[uiI][uiJ].uiY-stGame.uiCellHeight/2, stGame.uiCellWidth, stGame.uiCellHeight, RED);
        } else if (stGame.eTurn == PlayerTwo){
            vita2d_draw_rectangle(stGame.Field[uiI][uiJ].uiX-stGame.uiCellWidth/2, stGame.Field[uiI][uiJ].uiY-stGame.uiCellHeight/2, stGame.uiCellWidth, stGame.uiCellHeight, YELLOW);
        } else if (stGame.eTurn == PlayerThree){
            vita2d_draw_rectangle(stGame.Field[uiI][uiJ].uiX-stGame.uiCellWidth/2, stGame.Field[uiI][uiJ].uiY-stGame.uiCellHeight/2, stGame.uiCellWidth, stGame.uiCellHeight, BLUE);
        } else if (stGame.eTurn == PlayerFour){
            vita2d_draw_rectangle(stGame.Field[uiI][uiJ].uiX-stGame.uiCellWidth/2, stGame.Field[uiI][uiJ].uiY-stGame.uiCellHeight/2, stGame.uiCellWidth, stGame.uiCellHeight, GREEN);
        }

        // Draw dice
        vita2d_draw_rectangle(stGame.Field[stGame.uiFieldHeight/2][stGame.uiFieldWidth/2].uiX-stGame.uiCellWidth/2, stGame.Field[stGame.uiFieldHeight/2][stGame.uiFieldWidth/2].uiY-stGame.uiCellHeight/2, stGame.uiCellWidth, stGame.uiCellHeight, BLACK);
        vita2d_draw_rectangle(stGame.Field[stGame.uiFieldHeight/2][stGame.uiFieldWidth/2].uiX-stGame.uiCellWidth/2+2, stGame.Field[stGame.uiFieldHeight/2][stGame.uiFieldWidth/2].uiY-stGame.uiCellHeight/2+2, stGame.uiCellWidth-4, stGame.uiCellHeight-4, WHITE);

		for (int i=-1; i<2; i++){
			for (int j=-1; j<2; j++){
                if ((uiDice == 1 || uiDice == 3 || uiDice == 5) && j == 0 && i == 0){
                    vita2d_draw_fill_circle(stGame.Field[5][5].uiX+j*15, stGame.Field[5][5].uiY+i*15, 5, BLACK);
                }
                
                if ((uiDice == 2 || uiDice == 3) && ((j == -1 && i == 1) || (j == 1 && i == -1))){
                    vita2d_draw_fill_circle(stGame.Field[5][5].uiX+j*15, stGame.Field[5][5].uiY+i*15, 5, BLACK);
                }

                if ((uiDice == 4 || uiDice == 5 || uiDice == 6) && i != 0 && j != 0){
                    vita2d_draw_fill_circle(stGame.Field[5][5].uiX+j*15, stGame.Field[5][5].uiY+i*15, 5, BLACK);
                }

                if (uiDice == 6 && ((j == -1 && i == 0) || (j == 1 && i == 0))){
                    vita2d_draw_fill_circle(stGame.Field[5][5].uiX+j*15, stGame.Field[5][5].uiY+i*15, 5, BLACK);
                }
            }
        }

        // Draw board and pawns
		for (int i=0; i<stGame.uiFieldHeight; i++){
			for (int j=0; j<stGame.uiFieldWidth; j++){
                if (stGame.Field[i][j].eData != NoPosition){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2, BLACK);
                }
                
                if (stGame.Field[i][j].eData == Empty){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, WHITE);
				} else if (stGame.Field[i][j].eData == PlayerOne){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, RED);
                } else if (stGame.Field[i][j].eData == PlayerOneHome){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, RGBA8(255/2,   0,   0, 255));
				} else if (stGame.Field[i][j].eData == PlayerTwo){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, YELLOW);
                } else if (stGame.Field[i][j].eData == PlayerTwoHome){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, RGBA8(255/2, 255/2, 0, 255));
				} else if (stGame.Field[i][j].eData == PlayerThree){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, BLUE);
                } else if (stGame.Field[i][j].eData == PlayerThreeHome){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, RGBA8(0,   0,   255/2, 255));
				} else if (stGame.Field[i][j].eData == PlayerFour){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, GREEN);
                } else if (stGame.Field[i][j].eData == PlayerFourHome){
                    vita2d_draw_fill_circle(stGame.Field[i][j].uiX, stGame.Field[i][j].uiY, stGame.uiCellHeight/2*90/100, RGBA8(0,   255/2,   0, 255));
                }
			}
		}

        // Animate Pawn
        if (stPos.uiColIndex <= stGame.uiFieldWidth){
            static float rPosX;
            static float rPosY;
            static float rPsi;

            if (xAniInit){
                stAniPos = stOldPos;
                rPosX = stGame.Field[stAniPos.uiRowIndex][stAniPos.uiColIndex].uiX;
                rPosY = stGame.Field[stAniPos.uiRowIndex][stAniPos.uiColIndex].uiY;
                rPsi = atan2f((stGame.Field[stPos.uiRowIndex][stPos.uiColIndex].uiY-stGame.Field[stAniPos.uiRowIndex][stAniPos.uiColIndex].uiY),(stGame.Field[stPos.uiRowIndex][stPos.uiColIndex].uiX-stGame.Field[stAniPos.uiRowIndex][stAniPos.uiColIndex].uiX));
            }

            float rAniSpeed = 2.5; // 2.5 pixels per frame is approximately 150 pixels per sec
            float rDist = sqrtf(powf(stGame.Field[stPos.uiRowIndex][stPos.uiColIndex].uiX-rPosX, 2) + powf(stGame.Field[stPos.uiRowIndex][stPos.uiColIndex].uiY-rPosY, 2));
            rPosX += rAniSpeed * cosf(rPsi);
            rPosY += rAniSpeed * sinf(rPsi);

            if (stGame.eTurn == PlayerOne){
                vita2d_draw_fill_circle(rPosX, rPosY, stGame.uiCellHeight/2*90/100, RED);
            } else if (stGame.eTurn == PlayerTwo){
                vita2d_draw_fill_circle(rPosX, rPosY, stGame.uiCellHeight/2*90/100, YELLOW);
            } else if (stGame.eTurn == PlayerThree){
                vita2d_draw_fill_circle(rPosX, rPosY, stGame.uiCellHeight/2*90/100, BLUE);
            } else if (stGame.eTurn == PlayerFour){
                vita2d_draw_fill_circle(rPosX, rPosY, stGame.uiCellHeight/2*90/100, GREEN);
            }
            
            if (rDist < rAniSpeed){
                stGame.Field[stPos.uiRowIndex][stPos.uiColIndex].eData = stGame.eTurn; // Set current player to the new pos
                xAniDone = true;
            } else{
                xAniDone = false;
            }
        }

		vita2d_wait_rendering_done();
		vita2d_end_drawing();
		vita2d_swap_buffers();
        sceDisplayWaitVblankStart();
	}
	return 0;
}
