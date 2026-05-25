// Wait for Tauri to be ready
let invoke;

function initTauri() {
    if (window.__TAURI__ && window.__TAURI__.core) {
        invoke = window.__TAURI__.core.invoke;
        console.log('Tauri IPC ready');
        initUI();
        startEmulator();
    } else {
        console.log('Waiting for Tauri...');
        setTimeout(initTauri, 100);
    }
}

// Screen state
let screen = Array(24).fill('').map(() => ' '.repeat(40));
let cursorPos = { col: 0, row: 0 };
let cursorVisible = true;
let running = false;

// Canvas and rendering
const canvas = document.getElementById('crt');
const ctx = canvas.getContext('2d');

const CHAR_WIDTH = 14;
const CHAR_HEIGHT = 16;
const PHOSPHOR_GREEN = '#33ff33';
const BG_COLOR = '#001100';

let blinkCounter = 0;
const BLINK_RATE = 30;

// Demo programs (loaded from files)
const DEMOS = {
    'hello.bas': '10 PRINT "HELLO, APPLE 1 WORLD!"\n20 END\n',
    'fibonacci.bas': '10 REM ** FIBONACCI SEQUENCE **\n20 REM PRINTS FIRST 20 FIBONACCI NUMBERS\n30 REM USES INTEGER ARITHMETIC (16-BIT)\n40 PRINT "** FIBONACCI NUMBERS **"\n50 PRINT\n60 LET A = 0\n70 LET B = 1\n80 FOR I = 1 TO 20\n90 PRINT I;": ";A\n100 LET C = A + B\n110 LET A = B\n120 LET B = C\n130 NEXT I\n140 PRINT\n150 PRINT "DONE."\n160 END\n',
    'guess.bas': '10 REM ** NUMBER GUESSING GAME **\n20 REM COMPUTER PICKS 1-100, YOU GUESS\n30 PRINT "** NUMBER GUESSING GAME **"\n40 PRINT\n50 PRINT "I\'M THINKING OF A NUMBER"\n60 PRINT "BETWEEN 1 AND 100."\n70 PRINT\n80 LET N = RND(100) + 1\n90 LET T = 0\n100 PRINT "YOUR GUESS";\n110 INPUT G\n120 LET T = T + 1\n130 IF G = N THEN GOTO 180\n140 IF G < N THEN PRINT "TOO LOW!"\n150 IF G > N THEN PRINT "TOO HIGH!"\n160 GOTO 100\n170 REM ** WINNER **\n180 PRINT\n190 PRINT "CORRECT!"\n200 PRINT "YOU GOT IT IN ";T;" TRIES."\n210 PRINT\n220 PRINT "PLAY AGAIN? (1=YES, 0=NO)";\n230 INPUT A\n240 IF A = 1 THEN GOTO 40\n250 PRINT "THANKS FOR PLAYING!"\n260 END\n',
    'sieve.bas': '10 REM ** SIEVE OF ERATOSTHENES **\n20 REM FIND ALL PRIMES UP TO 100\n30 PRINT "** SIEVE OF ERATOSTHENES **"\n40 PRINT\n50 DIM A(100)\n60 REM INITIALIZE: 1=PRIME CANDIDATE\n70 FOR I = 2 TO 100\n80 LET A(I) = 1\n90 NEXT I\n100 REM SIEVE: MARK MULTIPLES\n110 FOR I = 2 TO 10\n120 IF A(I) = 0 THEN GOTO 170\n130 LET J = I * I\n140 IF J > 100 THEN GOTO 170\n150 LET A(J) = 0\n160 LET J = J + I\n165 IF J <= 100 THEN GOTO 150\n170 NEXT I\n180 REM PRINT PRIMES\n190 PRINT "PRIMES UP TO 100:"\n200 PRINT\n210 LET C = 0\n220 FOR I = 2 TO 100\n230 IF A(I) = 0 THEN GOTO 260\n240 PRINT I;" ";\n250 LET C = C + 1\n260 NEXT I\n270 PRINT\n280 PRINT\n290 PRINT C;" PRIMES FOUND."\n300 END\n',
    'adventure.bas': '10 REM ** MINI TEXT ADVENTURE **\n20 REM EXPLORE A DUNGEON, FIND THE GOLD\n30 PRINT "** DUNGEON ADVENTURE **"\n40 PRINT\n50 PRINT "FIND THE GOLD AND ESCAPE!"\n60 PRINT "COMMANDS: N S E W"\n70 PRINT\n80 LET R = 1\n90 LET K = 0\n100 LET G = 0\n110 REM ** MAIN LOOP **\n120 GOSUB 200\n130 PRINT "WHAT DO YOU DO";\n140 INPUT A$\n150 IF A$ = "N" THEN GOSUB 300\n160 IF A$ = "S" THEN GOSUB 320\n170 IF A$ = "E" THEN GOSUB 340\n180 IF A$ = "W" THEN GOSUB 360\n190 GOTO 120\n200 REM ** DESCRIBE ROOM **\n210 PRINT\n220 IF R = 1 THEN PRINT "ENTRANCE HALL. TORCHES FLICKER."\n230 IF R = 2 THEN PRINT "DARK CORRIDOR. COLD AIR."\n240 IF R = 3 THEN PRINT "ARMORY. A KEY GLINTS!"\n250 IF R = 4 THEN PRINT "LOCKED VAULT."\n260 IF R = 5 THEN PRINT "TREASURE ROOM! GOLD HERE!"\n270 IF R = 3 THEN IF K = 0 THEN LET K = 1\n280 IF R = 5 THEN IF G = 0 THEN LET G = 1\n290 IF R = 1 THEN IF G = 1 THEN GOTO 380\n295 RETURN\n300 REM ** MOVEMENT **\n310 IF R = 1 THEN LET R = 2\n312 IF R = 4 THEN LET R = 5\n315 RETURN\n320 IF R = 2 THEN LET R = 1\n322 IF R = 5 THEN LET R = 4\n325 RETURN\n340 IF R = 1 THEN LET R = 3\n342 IF R = 2 THEN LET R = 4\n344 IF R = 4 THEN IF K = 0 THEN PRINT "DOOR IS LOCKED!"\n346 IF R = 4 THEN IF K = 1 THEN LET R = 5\n348 RETURN\n360 IF R = 3 THEN LET R = 1\n362 IF R = 4 THEN LET R = 2\n364 IF R = 5 THEN LET R = 4\n366 RETURN\n380 REM ** WIN **\n390 PRINT\n400 PRINT "YOU ESCAPED WITH THE GOLD!"\n410 PRINT "*** YOU WIN! ***"\n420 END\n',
    'lunarlander.bas': '10 REM ** LUNAR LANDER **\n20 REM LAND YOUR SPACECRAFT SAFELY ON THE MOON\n30 PRINT "** LUNAR LANDER **"\n40 PRINT\n50 PRINT "WANT INSTRUCTIONS? (1=YES, 0=NO)";\n60 INPUT Q\n70 IF Q = 0 THEN GOTO 150\n80 PRINT\n90 PRINT "YOU ARE LANDING A SPACECRAFT ON"\n100 PRINT "THE MOON. CONTROL YOUR THRUST"\n110 PRINT "EACH SECOND (0-9). LAND WITH"\n120 PRINT "VELOCITY UNDER 5 TO SURVIVE!"\n130 PRINT "YOU HAVE 200 UNITS OF FUEL."\n140 PRINT\n150 REM ** INITIALIZE **\n160 LET A = 500\n170 LET V = 50\n180 LET F = 200\n190 LET T = 0\n200 REM ** MAIN LOOP **\n210 PRINT "-----------------------------"\n220 PRINT "TIME:";T;" ALT:";A;" VEL:";V\n230 PRINT "FUEL:";F\n240 IF F = 0 THEN PRINT "*** NO FUEL! ***"\n250 IF F = 0 THEN GOTO 310\n260 PRINT "THRUST (0-9)";\n270 INPUT B\n280 IF B < 0 THEN LET B = 0\n290 IF B > 9 THEN LET B = 9\n300 IF B > F THEN LET B = F\n310 REM ** PHYSICS **\n320 LET V = V + 5 - B\n330 LET A = A - V\n340 LET F = F - B\n350 LET T = T + 1\n360 IF A > 0 THEN GOTO 200\n370 REM ** LANDING CHECK **\n380 LET A = 0\n390 PRINT "-----------------------------"\n400 PRINT "*** CONTACT! ***"\n410 PRINT "VELOCITY AT IMPACT:";V\n420 IF V < 5 THEN GOTO 460\n430 PRINT "YOU CRASHED! TOO FAST!"\n440 PRINT "BETTER LUCK NEXT TIME."\n450 GOTO 490\n460 PRINT "PERFECT LANDING!"\n470 PRINT "CONGRATULATIONS!"\n480 PRINT "TIME ELAPSED:";T;" SECONDS"\n490 PRINT\n500 PRINT "PLAY AGAIN? (1=YES, 0=NO)";\n510 INPUT Q\n520 IF Q = 1 THEN GOTO 150\n530 PRINT "GOODBYE, ASTRONAUT!"\n540 END\n',
    'wumpus.bas': '10 REM ** HUNT THE WUMPUS **\n20 REM CLASSIC CAVE GAME - GREGORY YOB 1972\n30 PRINT "** HUNT THE WUMPUS **"\n40 PRINT\n50 PRINT "WANT INSTRUCTIONS? (1=YES, 0=NO)";\n60 INPUT Q\n70 IF Q = 0 THEN GOTO 170\n80 PRINT\n90 PRINT "A WUMPUS LIVES IN A CAVE OF 20"\n100 PRINT "ROOMS. EACH CONNECTS TO 3 OTHERS."\n110 PRINT "HAZARDS: 2 PITS, 2 BAT ROOMS."\n120 PRINT "SENSES WARN YOU OF NEARBY DANGER."\n130 PRINT "SHOOT ARROWS TO KILL THE WUMPUS!"\n140 PRINT "COMMANDS: M=MOVE, S=SHOOT"\n150 PRINT\n160 REM ** CAVE MAP - DODECAHEDRON **\n170 DIM C(20,3)\n180 FOR I = 1 TO 20\n190 READ C(I,1),C(I,2),C(I,3)\n200 NEXT I\n210 REM ** ADJACENCY DATA **\n220 DATA 2,5,8,1,3,10,2,4,12,3,5,14,4,1,6\n230 DATA 5,7,15,6,8,17,7,1,9,8,10,18,9,2,11\n240 DATA 10,12,19,11,3,13,12,14,20,13,4,15,14,6,16\n250 DATA 15,17,20,16,7,18,17,9,19,18,11,20,19,13,16\n260 REM ** PLACE THINGS **\n270 LET P = RND(20) + 1\n280 LET W = RND(20) + 1\n290 IF W = P THEN GOTO 280\n300 LET B1 = RND(20) + 1\n310 IF B1 = P THEN GOTO 300\n320 IF B1 = W THEN GOTO 300\n330 LET B2 = RND(20) + 1\n340 IF B2 = P THEN GOTO 330\n350 IF B2 = W THEN GOTO 330\n360 IF B2 = B1 THEN GOTO 330\n370 LET P1 = RND(20) + 1\n380 IF P1 = P THEN GOTO 370\n390 LET P2 = RND(20) + 1\n400 IF P2 = P THEN GOTO 390\n410 IF P2 = P1 THEN GOTO 390\n420 LET A = 5\n430 REM ** MAIN LOOP **\n440 PRINT\n450 PRINT "YOU ARE IN ROOM";P\n460 PRINT "TUNNELS LEAD TO";C(P,1);C(P,2);C(P,3)\n470 REM ** CHECK SENSES **\n480 FOR I = 1 TO 3\n490 LET R = C(P,I)\n500 IF R = W THEN PRINT "I SMELL A WUMPUS!"\n510 IF R = P1 THEN PRINT "I FEEL A DRAFT!"\n520 IF R = P2 THEN PRINT "I FEEL A DRAFT!"\n530 IF R = B1 THEN PRINT "BATS NEARBY!"\n540 IF R = B2 THEN PRINT "BATS NEARBY!"\n550 NEXT I\n560 PRINT "SHOOT OR MOVE (S/M)";\n570 INPUT A$\n580 IF A$ = "M" THEN GOTO 630\n590 IF A$ = "S" THEN GOTO 710\n600 GOTO 560\n610 REM ** MOVE **\n630 PRINT "WHERE TO";\n640 INPUT R\n650 IF R = C(P,1) THEN GOTO 670\n660 IF R = C(P,2) THEN GOTO 670\n665 IF R = C(P,3) THEN GOTO 670\n666 PRINT "NOT POSSIBLE!"\n667 GOTO 630\n670 LET P = R\n680 IF P = W THEN GOTO 820\n690 IF P = P1 THEN GOTO 840\n695 IF P = P2 THEN GOTO 840\n700 IF P = B1 THEN GOTO 860\n705 IF P = B2 THEN GOTO 860\n706 GOTO 440\n710 REM ** SHOOT **\n720 PRINT "SHOOT INTO WHICH ROOM";\n730 INPUT R\n740 LET A = A - 1\n750 IF R = W THEN GOTO 800\n760 PRINT "MISSED!"\n770 LET W = C(W,RND(3)+1)\n780 IF W = P THEN GOTO 820\n790 IF A = 0 THEN GOTO 880\n795 GOTO 440\n800 PRINT "*** YOU GOT THE WUMPUS! ***"\n810 GOTO 900\n820 PRINT "THE WUMPUS GOT YOU!"\n830 GOTO 900\n840 PRINT "FELL IN A PIT! AAAA..."\n850 GOTO 900\n860 PRINT "ZAP! BAT CARRIES YOU AWAY!"\n870 LET P = RND(20) + 1\n875 GOTO 670\n880 PRINT "OUT OF ARROWS! YOU LOSE."\n900 PRINT\n910 PRINT "PLAY AGAIN? (1=YES, 0=NO)";\n920 INPUT Q\n930 IF Q = 1 THEN GOTO 260\n940 END\n',
    'hamurabi.bas': '10 REM ** HAMURABI **\n20 REM RULE ANCIENT SUMERIA FOR 10 YEARS\n30 PRINT "** HAMURABI **"\n40 PRINT\n50 PRINT "WANT INSTRUCTIONS? (1=YES, 0=NO)";\n60 INPUT Q\n70 IF Q = 0 THEN GOTO 150\n80 PRINT\n90 PRINT "YOU GOVERN ANCIENT SUMERIA."\n100 PRINT "BUY/SELL LAND, FEED YOUR PEOPLE,"\n110 PRINT "AND PLANT CROPS EACH YEAR."\n120 PRINT "SURVIVE 10 YEARS TO WIN!"\n130 PRINT\n140 REM ** INITIALIZE **\n150 LET Y = 0\n160 LET P = 100\n170 LET S = 2800\n180 LET A = 1000\n190 LET D = 0\n200 LET H = 3\n210 LET E = 200\n220 LET TD = 0\n230 REM ** YEARLY REPORT **\n240 LET Y = Y + 1\n250 IF Y > 10 THEN GOTO 700\n260 PRINT\n270 PRINT "==== YEAR";Y;"===="\n280 PRINT "POPULATION:";P\n290 PRINT "ACRES:";A\n300 PRINT "GRAIN IN STORE:";S\n310 IF D > 0 THEN PRINT D;" PEOPLE STARVED"\n320 LET L = RND(10) + 17\n330 PRINT "LAND COSTS";L;" BUSHELS/ACRE"\n340 REM ** BUY LAND **\n350 PRINT "ACRES TO BUY";\n360 INPUT B\n370 IF B < 0 THEN GOTO 350\n380 IF B * L > S THEN PRINT "NOT ENOUGH GRAIN!"\n385 IF B * L > S THEN GOTO 350\n390 LET A = A + B\n400 LET S = S - B * L\n410 IF B > 0 THEN GOTO 450\n420 REM ** SELL LAND **\n430 PRINT "ACRES TO SELL";\n440 INPUT B\n445 IF B < 0 THEN GOTO 430\n446 IF B > A THEN PRINT "NOT ENOUGH LAND!"\n447 IF B > A THEN GOTO 430\n448 LET A = A - B\n449 LET S = S + B * L\n450 REM ** FEED PEOPLE **\n460 PRINT "BUSHELS TO FEED PEOPLE";\n470 INPUT F\n480 IF F < 0 THEN GOTO 460\n490 IF F > S THEN PRINT "NOT ENOUGH GRAIN!"\n495 IF F > S THEN GOTO 460\n500 LET S = S - F\n510 REM ** PLANT CROPS **\n520 PRINT "ACRES TO PLANT";\n530 INPUT C\n540 IF C < 0 THEN GOTO 520\n550 IF C > A THEN PRINT "NOT ENOUGH LAND!"\n555 IF C > A THEN GOTO 520\n560 IF C > S * 2 THEN PRINT "NOT ENOUGH GRAIN!"\n565 IF C > S * 2 THEN GOTO 520\n570 IF C > P * 10 THEN PRINT "NOT ENOUGH PEOPLE!"\n575 IF C > P * 10 THEN GOTO 520\n580 LET S = S - C / 2\n590 REM ** HARVEST **\n600 LET H = RND(5) + 1\n610 LET S = S + H * C\n620 REM ** RATS **\n630 IF RND(2) = 1 THEN LET E = S / (RND(3) + 3)\n635 IF RND(2) = 1 THEN LET S = S - E\n640 REM ** STARVATION **\n650 LET D = P - F / 20\n660 IF D < 0 THEN LET D = 0\n670 LET TD = TD + D\n680 LET P = P - D\n690 IF P <= 0 THEN GOTO 770\n695 REM ** IMMIGRANTS **\n696 LET I = RND(5) + 1\n697 LET P = P + I\n698 REM ** PLAGUE **\n699 IF RND(100) < 15 THEN LET P = P / 2\n700 IF Y <= 10 THEN GOTO 240\n710 REM ** FINAL REPORT **\n720 PRINT\n730 PRINT "==== FINAL REPORT ===="\n740 PRINT "YOU RULED 10 YEARS."\n750 PRINT "POPULATION:";P\n760 PRINT "ACRES:";A;" GRAIN:";S\n762 PRINT "TOTAL STARVED:";TD\n764 IF TD < 30 THEN PRINT "SUPERB LEADERSHIP!"\n766 IF TD > 30 THEN IF TD < 80 THEN PRINT "FAIR PERFORMANCE."\n768 IF TD > 80 THEN PRINT "THE PEOPLE REVOLTED!"\n769 GOTO 790\n770 PRINT\n780 PRINT "EVERYONE STARVED! YOU LOSE!"\n790 PRINT\n800 PRINT "PLAY AGAIN? (1=YES, 0=NO)";\n810 INPUT Q\n820 IF Q = 1 THEN GOTO 150\n830 END\n',
    'life.bas': '10 REM ** CONWAY\'S GAME OF LIFE **\n20 REM CELLULAR AUTOMATON ON 20X20 GRID\n30 PRINT "** GAME OF LIFE **"\n40 PRINT\n50 PRINT "WANT INSTRUCTIONS? (1=YES, 0=NO)";\n60 INPUT Q\n70 IF Q = 0 THEN GOTO 140\n80 PRINT\n90 PRINT "CELLS LIVE OR DIE BY RULES:"\n100 PRINT " 2-3 NEIGHBORS: SURVIVES"\n110 PRINT " 3 NEIGHBORS: BIRTH"\n120 PRINT " ELSE: DIES"\n130 PRINT "ENTER CELLS, THEN WATCH!"\n140 PRINT\n150 DIM A(22,22)\n160 DIM B(22,22)\n170 REM ** CLEAR GRID **\n180 FOR I = 0 TO 21\n190 FOR J = 0 TO 21\n200 LET A(I,J) = 0\n210 LET B(I,J) = 0\n220 NEXT J\n230 NEXT I\n240 REM ** INPUT CELLS **\n250 PRINT "ENTER LIVE CELLS (ROW,COL)"\n260 PRINT "ROW 1-20, COL 1-20"\n270 PRINT "ENTER 0,0 WHEN DONE"\n280 PRINT "ROW,COL";\n290 INPUT R,C\n300 IF R = 0 THEN GOTO 340\n310 IF R < 1 THEN GOTO 280\n320 IF R > 20 THEN GOTO 280\n325 IF C < 1 THEN GOTO 280\n330 IF C > 20 THEN GOTO 280\n335 LET A(R,C) = 1\n336 GOTO 280\n340 PRINT "HOW MANY GENERATIONS";\n350 INPUT G\n360 REM ** DISPLAY GRID **\n370 PRINT\n380 FOR I = 1 TO 20\n390 LET L$ = ""\n400 FOR J = 1 TO 20\n410 IF A(I,J) = 1 THEN LET L$ = L$ + "*"\n420 IF A(I,J) = 0 THEN LET L$ = L$ + "."\n430 NEXT J\n440 PRINT L$\n450 NEXT I\n460 LET G = G - 1\n470 IF G < 0 THEN GOTO 620\n480 REM ** COMPUTE NEXT GEN **\n490 FOR I = 1 TO 20\n500 FOR J = 1 TO 20\n510 LET N = A(I-1,J-1)+A(I-1,J)+A(I-1,J+1)\n520 LET N = N+A(I,J-1)+A(I,J+1)\n530 LET N = N+A(I+1,J-1)+A(I+1,J)+A(I+1,J+1)\n540 LET B(I,J) = 0\n550 IF A(I,J) = 1 THEN IF N = 2 THEN LET B(I,J) = 1\n560 IF N = 3 THEN LET B(I,J) = 1\n570 NEXT J\n580 NEXT I\n590 REM ** COPY B TO A **\n600 FOR I = 1 TO 20\n610 FOR J = 1 TO 20\n615 LET A(I,J) = B(I,J)\n616 NEXT J\n617 NEXT I\n618 GOTO 370\n620 PRINT\n630 PRINT "DONE. PLAY AGAIN? (1=YES, 0=NO)";\n640 INPUT Q\n650 IF Q = 1 THEN GOTO 170\n660 END\n',
    'startrek.bas': '10 REM ** STAR TREK **\n20 REM DESTROY ALL KLINGONS IN 30 STARDATES\n30 PRINT "** STAR TREK **"\n40 PRINT\n50 PRINT "WANT INSTRUCTIONS? (1=YES, 0=NO)";\n60 INPUT Q\n70 IF Q = 0 THEN GOTO 160\n80 PRINT\n90 PRINT "DESTROY ALL KLINGONS IN 30 DATES!"\n100 PRINT "COMMANDS:"\n110 PRINT " SRS - SHORT RANGE SCAN"\n120 PRINT " LRS - LONG RANGE SCAN"\n130 PRINT " NAV - NAVIGATE (MOVE)"\n140 PRINT " PHA - FIRE PHASERS"\n150 PRINT " TOR - FIRE TORPEDO"\n160 REM ** INIT GALAXY **\n170 DIM G(4,4)\n180 DIM K(4,4)\n190 LET TK = 0\n200 LET E = 3000\n210 LET T = 30\n220 LET QR = RND(4) + 1\n230 LET QC = RND(4) + 1\n240 LET SR = RND(8) + 1\n250 LET SC = RND(8) + 1\n260 REM ** POPULATE GALAXY **\n270 FOR I = 1 TO 4\n280 FOR J = 1 TO 4\n290 LET K(I,J) = 0\n300 IF RND(100) < 25 THEN LET K(I,J) = RND(3) + 1\n310 LET TK = TK + K(I,J)\n320 LET G(I,J) = K(I,J) * 100 + RND(5) * 10 + RND(4)\n330 NEXT J\n340 NEXT I\n350 IF TK = 0 THEN LET K(1,1) = 2\n355 IF TK = 0 THEN LET TK = 2\n360 PRINT "MISSION: DESTROY";TK;"KLINGONS"\n370 PRINT "ENERGY:";E;" STARDATES LEFT:";T\n380 REM ** COMMAND LOOP **\n390 PRINT\n400 PRINT "COMMAND (SRS/LRS/NAV/PHA/TOR)";\n410 INPUT A$\n420 IF A$ = "SRS" THEN GOTO 500\n430 IF A$ = "LRS" THEN GOTO 570\n440 IF A$ = "NAV" THEN GOTO 630\n450 IF A$ = "PHA" THEN GOTO 700\n460 IF A$ = "TOR" THEN GOTO 760\n470 PRINT "UNKNOWN COMMAND"\n480 GOTO 390\n490 REM ** SHORT RANGE SCAN **\n500 PRINT "SECTOR [";QR;",";QC;"]"\n510 PRINT "POSITION:";SR;",";SC\n520 PRINT "ENERGY:";E\n530 PRINT "KLINGONS HERE:";K(QR,QC)\n540 PRINT "STARDATES LEFT:";T\n550 GOTO 390\n560 REM ** LONG RANGE SCAN **\n570 PRINT "LONG RANGE SCAN:"\n580 FOR I = 1 TO 4\n590 FOR J = 1 TO 4\n600 PRINT G(I,J);\n610 NEXT J\n620 PRINT\n625 NEXT I\n626 GOTO 390\n630 REM ** NAVIGATE **\n640 PRINT "QUADRANT ROW (1-4)";\n650 INPUT NR\n660 PRINT "QUADRANT COL (1-4)";\n670 INPUT NC\n680 IF NR < 1 THEN GOTO 390\n685 IF NR > 4 THEN GOTO 390\n686 IF NC < 1 THEN GOTO 390\n687 IF NC > 4 THEN GOTO 390\n688 LET QR = NR\n689 LET QC = NC\n690 LET E = E - 100\n691 LET T = T - 1\n692 IF T <= 0 THEN GOTO 850\n693 IF E <= 0 THEN GOTO 870\n694 PRINT "ARRIVED AT QUADRANT";QR;",";QC\n695 GOTO 390\n700 REM ** PHASERS **\n710 IF K(QR,QC) = 0 THEN PRINT "NO TARGETS"\n715 IF K(QR,QC) = 0 THEN GOTO 390\n720 PRINT "ENERGY TO FIRE";\n730 INPUT F\n740 IF F > E THEN PRINT "NOT ENOUGH!"\n745 IF F > E THEN GOTO 390\n750 LET E = E - F\n752 IF F > K(QR,QC) * 200 THEN GOTO 755\n753 PRINT "KLINGON DAMAGED BUT ALIVE!"\n754 GOTO 390\n755 PRINT "KLINGON DESTROYED!"\n756 LET TK = TK - 1\n757 LET K(QR,QC) = K(QR,QC) - 1\n758 IF TK = 0 THEN GOTO 830\n759 GOTO 390\n760 REM ** TORPEDO **\n770 IF K(QR,QC) = 0 THEN PRINT "NO TARGETS"\n775 IF K(QR,QC) = 0 THEN GOTO 390\n780 LET E = E - 50\n790 IF RND(100) < 60 THEN GOTO 810\n800 PRINT "TORPEDO MISSED!"\n805 GOTO 390\n810 PRINT "*** DIRECT HIT! ***"\n815 LET K(QR,QC) = K(QR,QC) - 1\n820 LET TK = TK - 1\n825 IF TK = 0 THEN GOTO 830\n826 GOTO 390\n830 PRINT\n840 PRINT "*** CONGRATULATIONS! ***"\n842 PRINT "ALL KLINGONS DESTROYED!"\n844 GOTO 880\n850 PRINT\n860 PRINT "TIME\'S UP! KLINGONS WIN!"\n862 GOTO 880\n870 PRINT "OUT OF ENERGY! STRANDED!"\n880 PRINT "PLAY AGAIN? (1=YES, 0=NO)";\n890 INPUT Q\n900 IF Q = 1 THEN GOTO 160\n910 END\n',
    'nim.bas': '10 REM ** NIM **\n20 REM CLASSIC MATCHSTICK GAME\n30 PRINT "** NIM **"\n40 PRINT\n50 PRINT "WANT INSTRUCTIONS? (1=YES, 0=NO)";\n60 INPUT Q\n70 IF Q = 0 THEN GOTO 140\n80 PRINT\n90 PRINT "THERE ARE 21 STICKS."\n100 PRINT "TAKE 1, 2, OR 3 EACH TURN."\n110 PRINT "WHOEVER TAKES THE LAST STICK"\n120 PRINT "LOSES! YOU GO FIRST."\n130 PRINT\n140 REM ** START GAME **\n150 LET S = 21\n160 PRINT\n170 PRINT "STICKS REMAINING:";S\n180 REM ** DRAW STICKS **\n190 LET L$ = ""\n200 FOR I = 1 TO S\n210 LET L$ = L$ + "I"\n220 NEXT I\n230 PRINT L$\n240 PRINT\n250 REM ** PLAYER TURN **\n260 PRINT "TAKE HOW MANY (1-3)";\n270 INPUT P\n280 IF P < 1 THEN PRINT "MUST TAKE AT LEAST 1!"\n285 IF P < 1 THEN GOTO 260\n290 IF P > 3 THEN PRINT "3 IS THE MAX!"\n295 IF P > 3 THEN GOTO 260\n300 IF P > S THEN PRINT "NOT THAT MANY LEFT!"\n305 IF P > S THEN GOTO 260\n310 LET S = S - P\n320 IF S <= 0 THEN GOTO 430\n330 PRINT "YOU TOOK";P;". LEFT:";S\n340 REM ** COMPUTER TURN **\n350 REM ** OPTIMAL: LEAVE MULTIPLE OF 4 + 1 **\n360 LET C = S - 1\n370 LET C = C - (C / 4) * 4\n380 IF C = 0 THEN LET C = RND(3) + 1\n390 IF C > S THEN LET C = S\n400 LET S = S - C\n410 PRINT "I TAKE";C;". LEFT:";S\n420 IF S <= 0 THEN GOTO 450\n425 GOTO 160\n430 REM ** PLAYER TOOK LAST **\n440 PRINT "YOU TOOK THE LAST STICK!"\n442 PRINT "*** I WIN! ***"\n444 GOTO 460\n450 PRINT "I TOOK THE LAST STICK."\n452 PRINT "*** YOU WIN! ***"\n460 PRINT\n470 PRINT "PLAY AGAIN? (1=YES, 0=NO)";\n480 INPUT Q\n490 IF Q = 1 THEN GOTO 140\n500 PRINT "THANKS FOR PLAYING!"\n510 END\n',
    'mastermind.bas': '10 REM ** MASTERMIND **\n20 REM BULLS AND COWS - BREAK THE CODE\n30 PRINT "** MASTERMIND **"\n40 PRINT\n50 PRINT "WANT INSTRUCTIONS? (1=YES, 0=NO)";\n60 INPUT Q\n70 IF Q = 0 THEN GOTO 160\n80 PRINT\n90 PRINT "I HAVE PICKED 4 UNIQUE DIGITS"\n100 PRINT "FROM 1-6. GUESS MY CODE!"\n110 PRINT "BULLS = RIGHT DIGIT, RIGHT PLACE"\n120 PRINT "COWS = RIGHT DIGIT, WRONG PLACE"\n130 PRINT "YOU HAVE 10 GUESSES."\n140 PRINT\n150 REM ** GENERATE SECRET **\n160 DIM S(4)\n170 DIM G(4)\n180 LET S(1) = RND(6) + 1\n190 LET S(2) = RND(6) + 1\n200 IF S(2) = S(1) THEN GOTO 190\n210 LET S(3) = RND(6) + 1\n220 IF S(3) = S(1) THEN GOTO 210\n230 IF S(3) = S(2) THEN GOTO 210\n240 LET S(4) = RND(6) + 1\n250 IF S(4) = S(1) THEN GOTO 240\n260 IF S(4) = S(2) THEN GOTO 240\n270 IF S(4) = S(3) THEN GOTO 240\n280 LET T = 0\n290 REM ** GUESS LOOP **\n300 LET T = T + 1\n310 IF T > 10 THEN GOTO 600\n320 PRINT\n330 PRINT "GUESS #";T\n340 PRINT "ENTER 4 DIGITS (1-6)"\n350 PRINT "DIGIT 1";\n360 INPUT G(1)\n370 PRINT "DIGIT 2";\n380 INPUT G(2)\n390 PRINT "DIGIT 3";\n400 INPUT G(3)\n410 PRINT "DIGIT 4";\n420 INPUT G(4)\n430 REM ** COUNT BULLS **\n440 LET B = 0\n450 LET C = 0\n460 FOR I = 1 TO 4\n470 IF G(I) = S(I) THEN LET B = B + 1\n480 NEXT I\n490 REM ** COUNT COWS **\n500 FOR I = 1 TO 4\n510 FOR J = 1 TO 4\n520 IF I = J THEN GOTO 550\n530 IF G(I) = S(J) THEN LET C = C + 1\n550 NEXT J\n560 NEXT I\n570 PRINT "BULLS:";B;" COWS:";C\n580 IF B = 4 THEN GOTO 630\n590 GOTO 300\n600 REM ** LOST **\n610 PRINT "OUT OF GUESSES!"\n620 PRINT "CODE WAS:";S(1);S(2);S(3);S(4)\n625 GOTO 650\n630 PRINT\n640 PRINT "*** YOU CRACKED THE CODE! ***"\n645 PRINT "IN";T;"GUESSES!"\n650 PRINT\n660 PRINT "PLAY AGAIN? (1=YES, 0=NO)";\n670 INPUT Q\n680 IF Q = 1 THEN GOTO 160\n690 END\n',
    'star.bas': '10 REM ** STAR PATTERN **\n20 REM DRAWS A DIAMOND OF STARS\n30 PRINT "** STAR DIAMOND PATTERN **"\n40 PRINT\n50 LET S = 10\n60 REM TOP HALF\n70 FOR I = 1 TO S\n80 FOR J = 1 TO S - I\n90 PRINT " ";\n100 NEXT J\n110 FOR J = 1 TO 2 * I - 1\n120 PRINT "*";\n130 NEXT J\n140 PRINT\n150 NEXT I\n160 REM BOTTOM HALF\n170 FOR I = S - 1 TO 1 STEP -1\n180 FOR J = 1 TO S - I\n190 PRINT " ";\n200 NEXT J\n210 FOR J = 1 TO 2 * I - 1\n220 PRINT "*";\n230 NEXT J\n240 PRINT\n250 NEXT I\n260 PRINT\n270 PRINT "SIZE (1-12, 0=QUIT)";\n280 INPUT S\n290 IF S = 0 THEN GOTO 310\n300 GOTO 60\n310 END\n',
    'sort.bas': '10 REM ** BUBBLE SORT DEMO **\n20 REM SORTS 10 RANDOM NUMBERS\n30 PRINT "** BUBBLE SORT DEMO **"\n40 PRINT\n50 DIM A(10)\n60 REM FILL WITH RANDOM NUMBERS\n70 PRINT "UNSORTED: ";\n80 FOR I = 1 TO 10\n90 LET A(I) = RND(99) + 1\n100 PRINT A(I);" ";\n110 NEXT I\n120 PRINT\n130 PRINT\n140 REM BUBBLE SORT\n150 PRINT "SORTING..."\n160 FOR I = 1 TO 9\n170 FOR J = 1 TO 10 - I\n180 IF A(J) <= A(J + 1) THEN GOTO 220\n190 LET T = A(J)\n200 LET A(J) = A(J + 1)\n210 LET A(J + 1) = T\n220 NEXT J\n230 NEXT I\n240 REM PRINT RESULT\n250 PRINT\n260 PRINT "SORTED:   ";\n270 FOR I = 1 TO 10\n280 PRINT A(I);" ";\n290 NEXT I\n300 PRINT\n310 PRINT\n320 PRINT "AGAIN? (1=YES, 0=NO)";\n330 INPUT A\n340 IF A = 1 THEN GOTO 40\n350 END\n',
    'calendar.bas': '10 REM ** CALENDAR PRINTER **\n20 REM PRINTS MONTH CALENDAR\n30 PRINT "** CALENDAR **"\n40 PRINT\n50 PRINT "MONTH (1-12)";\n60 INPUT M\n70 PRINT "YEAR";\n80 INPUT Y\n90 REM DAYS IN MONTH\n100 DIM D(12)\n110 LET D(1) = 31\n120 LET D(2) = 28\n130 LET D(3) = 31\n140 LET D(4) = 30\n150 LET D(5) = 31\n160 LET D(6) = 30\n170 LET D(7) = 31\n180 LET D(8) = 31\n190 LET D(9) = 30\n200 LET D(10) = 31\n210 LET D(11) = 30\n220 LET D(12) = 31\n230 REM LEAP YEAR CHECK\n240 IF M <> 2 THEN GOTO 270\n250 LET L = Y - (Y / 4) * 4\n260 IF L = 0 THEN LET D(2) = 29\n270 REM ZELLERS FORMULA FOR DAY OF WEEK\n280 REM FIND WHAT DAY THE 1ST FALLS ON\n290 LET A = (14 - M) / 12\n300 LET B = Y - A\n310 LET C = M + 12 * A - 2\n320 LET W = (1 + B + B / 4 - B / 100 + B / 400 + (31 * C) / 12)\n330 LET W = W - (W / 7) * 7\n340 REM PRINT HEADER\n350 PRINT\n360 PRINT " SU MO TU WE TH FR SA"\n370 PRINT " ---------------------"\n380 REM LEADING SPACES\n390 FOR I = 1 TO W\n400 PRINT "   ";\n410 NEXT I\n420 REM PRINT DAYS\n430 FOR I = 1 TO D(M)\n440 IF I < 10 THEN PRINT "  ";I;\n450 IF I >= 10 THEN PRINT " ";I;\n460 LET W = W + 1\n470 IF W = 7 THEN PRINT\n480 IF W = 7 THEN LET W = 0\n490 NEXT I\n500 PRINT\n510 END\n',
};

async function initUI() {
    // BASIC button
    document.getElementById('btn-basic').addEventListener('click', async () => {
        const cmd = 'E000R\r';
        for (const ch of cmd) {
            const code = ch === '\r' ? 0x0D : ch.charCodeAt(0);
            await invoke('send_key', { ch: code });
            // Small delay for processing
            await new Promise(r => setTimeout(r, 50));
        }
    });

    // About button
    document.getElementById('btn-about').addEventListener('click', () => {
        document.getElementById('about-modal').classList.remove('hidden');
    });
    document.getElementById('about-close').addEventListener('click', () => {
        document.getElementById('about-modal').classList.add('hidden');
    });
    document.getElementById('about-modal').addEventListener('click', (e) => {
        if (e.target === e.currentTarget) {
            e.currentTarget.classList.add('hidden');
        }
    });

    // Demo programs dropdown
    const demoMenu = document.getElementById('demo-menu');
    const btnDemos = document.getElementById('btn-demos');

    btnDemos.addEventListener('click', () => {
        demoMenu.classList.toggle('show');
    });

    // Close dropdown when clicking elsewhere
    document.addEventListener('click', (e) => {
        if (!e.target.closest('.dropdown')) {
            demoMenu.classList.remove('show');
        }
    });

    // Populate demo menu
    const demoList = [
        ['hello.bas', 'Hello World'],
        ['fibonacci.bas', 'Fibonacci Numbers'],
        ['guess.bas', 'Number Guessing Game'],
        ['sieve.bas', 'Sieve of Eratosthenes'],
        ['adventure.bas', 'Text Adventure'],
        ['lunarlander.bas', 'Lunar Lander'],
        ['wumpus.bas', 'Hunt the Wumpus'],
        ['hamurabi.bas', 'Hamurabi (Kingdom)'],
        ['life.bas', "Conway's Game of Life"],
        ['startrek.bas', 'Star Trek'],
        ['nim.bas', 'Nim (Strategy)'],
        ['mastermind.bas', 'Mastermind'],
        ['star.bas', 'Diamond Pattern'],
        ['sort.bas', 'Bubble Sort'],
        ['calendar.bas', 'Calendar'],
    ];

    for (const [file, name] of demoList) {
        const btn = document.createElement('button');
        btn.textContent = name;
        btn.addEventListener('click', async () => {
            demoMenu.classList.remove('show');
            await loadProgram(file);
        });
        demoMenu.appendChild(btn);
    }
}

async function loadProgram(filename) {
    const program = DEMOS[filename];
    if (!program) {
        console.error('Demo not found:', filename);
        return;
    }
    try {
        await invoke('load_demo', { program });
    } catch (e) {
        console.error('Failed to load demo:', e);
    }
}

async function startEmulator() {
    running = true;
    console.log('Emulator loop started');
    requestAnimationFrame(loop);
}

async function loop() {
    if (!running) return;

    try {
        await invoke('step_frame');
        screen = await invoke('get_screen');
        const cursor = await invoke('get_cursor');
        cursorPos = { col: cursor[0], row: cursor[1] };
    } catch (e) {
        console.error('Frame error:', e);
    }

    blinkCounter++;
    if (blinkCounter >= BLINK_RATE) {
        cursorVisible = !cursorVisible;
        blinkCounter = 0;
    }

    render();
    requestAnimationFrame(loop);
}

function render() {
    ctx.fillStyle = BG_COLOR;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.font = `bold ${CHAR_HEIGHT - 2}px "Courier New", monospace`;
    ctx.textBaseline = 'top';

    for (let row = 0; row < 24; row++) {
        for (let col = 0; col < 40; col++) {
            let ch = (screen[row] && screen[row][col]) ? screen[row][col] : ' ';

            if (row === cursorPos.row && col === cursorPos.col) {
                ch = cursorVisible ? '@' : ' ';
            }

            if (ch && ch !== ' ') {
                const x = col * CHAR_WIDTH + 2;
                const y = row * CHAR_HEIGHT + 1;

                ctx.shadowColor = PHOSPHOR_GREEN;
                ctx.shadowBlur = 3;
                ctx.fillStyle = PHOSPHOR_GREEN;
                ctx.fillText(ch, x, y);
                ctx.shadowBlur = 0;
            }
        }
    }

    // Scanlines
    ctx.fillStyle = 'rgba(0, 0, 0, 0.04)';
    for (let y = 0; y < canvas.height; y += 2) {
        ctx.fillRect(0, y, canvas.width, 1);
    }

    // Vignette
    const grad = ctx.createRadialGradient(
        canvas.width / 2, canvas.height / 2, canvas.width * 0.35,
        canvas.width / 2, canvas.height / 2, canvas.width * 0.65
    );
    grad.addColorStop(0, 'rgba(0,0,0,0)');
    grad.addColorStop(1, 'rgba(0,0,0,0.2)');
    ctx.fillStyle = grad;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
}

// Keyboard
document.addEventListener('keydown', async (e) => {
    if (!invoke) return;
    // Don't capture keys when modal is open
    if (!document.getElementById('about-modal').classList.contains('hidden')) return;

    e.preventDefault();

    if (e.ctrlKey) {
        switch (e.key.toLowerCase()) {
            case 'c': await invoke('send_break'); return;
            case 'r': await invoke('reset'); return;
        }
    }

    let ch = 0;
    if (e.key === 'Enter') ch = 0x0D;
    else if (e.key === 'Backspace') ch = 0x5F;
    else if (e.key === 'Escape') ch = 0x1B;
    else if (e.key.length === 1) {
        ch = e.key.charCodeAt(0);
        if (ch >= 0x61 && ch <= 0x7A) ch -= 0x20;
        if (ch < 0x20 || ch > 0x5F) return;
    } else return;

    try {
        await invoke('send_key', { ch });
    } catch (e) {
        console.error('Key error:', e);
    }
});

// Boot
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => { render(); initTauri(); });
} else {
    render();
    initTauri();
}
