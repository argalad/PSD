#include "serverGame.h"
#include <pthread.h>

void showError (const char *msg)
{
  perror (msg);
  exit (0);
}

/*Descomentar para implementación de threads*/
// void *threadProcessing(void *threadArgs)
// {
//   tSession session;				         /** Session of this game */
//   int socketPlayer1;				         /** Socket descriptor for player 1 */
//   int socketPlayer2;				         /** Socket descriptor for player 2 */
//   tPlayer currentPlayer;			       /** Current player */
//   int endOfGame;				             /** Flag to control the end of the game*/
//   unsigned int card;				         /** Current card */
//   unsigned int code, codeRival;	   /** Codes for the active player and the rival */

//   // Get sockets for players
//   socketPlayer1 = ((tThreadArgs *) threadArgs)->socketPlayer1;
//   socketPlayer2 = ((tThreadArgs *) threadArgs)->socketPlayer2;

//   // Receive player 1 info


//   // Receive player 2 info

//   // Init...
//   endOfGame = FALSE;

//   while (!endOfGame)
//   {


//   }

//   // Close sockets
//   close (socketPlayer1);
//   close (socketPlayer2);

//   return (NULL) ;
// }

void send_deck (int socketPlayer, tDeck *deck)
{
  int msgLength;

  msgLength = send (socketPlayer, &(deck->numCards), sizeof (unsigned int), 0);
  if (msgLength < 0)
    showError ("Error while sending numCards to player.");
  msgLength = send (socketPlayer, deck->cards, sizeof (unsigned int) * deck->numCards, 0);
  if (msgLength < 0)
    showError ("Error while sending deck to player.");
}

void manage_bet (int socketPlayer, unsigned int *stack, unsigned int *bet)
{
  unsigned int code;
  int msgLength;

  // a. Send TURN_BET and stack to player
  code = TURN_BET;
  msgLength = send (socketPlayer, &code, sizeof(code), 0);
  if (msgLength < 0)
    showError ("ERROR while writing code to active player.");

  code = *stack;
  msgLength = send (socketPlayer, &code, sizeof(code), 0);
  if (msgLength < 0)
    showError ("ERROR while writing stack to active player.");

  while (code != TURN_BET_OK)
  {
    // Read
    msgLength = recv (socketPlayer, bet, sizeof(unsigned int), 0);
    if (msgLength < 0)
      showError ("ERROR while receiving player's bet.");

    if (*bet > 0 && *bet <= *stack 
      && *bet <= MAX_BET)
      code = TURN_BET_OK;
    else
      code = TURN_BET;

    // Send
    msgLength = send (socketPlayer, &code, sizeof (code), 0);
    if (msgLength < 0)
      showError ("ERROR while writing code to active player.");
  }

  // Update the stack
  *stack -= *bet;
}

void turn (int activePlayer, int inactivePlayer, tSession *session, tDeck *deck)
{
  int msgLength;
  unsigned int code, codeRival, points;

  // d. Start game
  // Get active player's deck
  for (int i = 0; i < 2; i++)
  {
    deck->cards[i] = getRandomCard (&session->gameDeck);
    deck->numCards++;
  }
  // i. ii.
  // Send TURN_PLAY to active player
  code = TURN_PLAY;
  msgLength = send (activePlayer, &code, sizeof (code), 0);
  if (msgLength < 0)
    showError ("ERROR while writing code to active player.");

  // Send TURN_PLAY_WAIT to inactive player
  codeRival = TURN_PLAY_WAIT;
  msgLength = send (inactivePlayer, &codeRival, sizeof (codeRival), 0);
  if (msgLength < 0)
    showError ("ERROR while writing code to inactive player.");

  // Send deck to both players
  send_deck (activePlayer, deck);
  send_deck (inactivePlayer, deck);
  printSession (session);

  // Get active player's points
  points = calculatePoints (deck);

  // Send points to both players
  msgLength = send (activePlayer, &points, sizeof (points), 0);
  if (msgLength < 0)
    showError ("ERROR while sending points to active player.");
  msgLength = send (inactivePlayer, &points, sizeof (points), 0);
  if (msgLength < 0)
    showError ("ERROR while sending points to inactive player.");

  // Answer from active player choice: HIT or STAND
  msgLength = recv (activePlayer, &code, sizeof (code), 0);
  if (msgLength < 0)
    showError ("ERROR while receiving active player's option.");

  // iii.
  while (code == TURN_PLAY_HIT)
  {
    deck->cards[deck->numCards++] = getRandomCard (&session->gameDeck);
    points = calculatePoints (deck);
    printSession (session);

    // Check player points
    if (points > 21)
    {
      code = TURN_PLAY_OUT;
      msgLength = send (activePlayer, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to active player.");

    }
    else
    {
      code = TURN_PLAY;
      msgLength = send (activePlayer, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to active player.");
    }

    code = TURN_PLAY_WAIT;
    msgLength = send (inactivePlayer, &code, sizeof (code), 0);
    if (msgLength < 0)
      showError ("ERROR while writing code to inactive player.");

    send_deck (activePlayer, deck);
    send_deck (inactivePlayer, deck);

    // Send points to both players
    msgLength = send (activePlayer, &points, sizeof (points), 0);
    if (msgLength < 0)
      showError ("ERROR while sending points to active player.");
    msgLength = send (inactivePlayer, &points, sizeof (points), 0);
    if (msgLength < 0)
      showError ("ERROR while sending points to inactive player.");

    if (!(points > 21))
    {
      // Answer from player 1 choice: HIT or STAND
      msgLength = recv (activePlayer, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while receiving active player's option.");
    }
  }

  codeRival = TURN_PLAY_RIVAL_DONE;
  msgLength = send (inactivePlayer, &codeRival, sizeof (codeRival), 0);
  if (msgLength < 0)
    showError ("ERROR while writing code to inactive player.");
}

int main (int argc, char *argv[])
{
  tSession session;                       /** Session of this game */
  tPlayer currentPlayer;                  /** Current player */
  int endOfGame;                          /** Flag to control the end of the game*/
  unsigned int card;                      /** Current card */
  unsigned int code, codeRival;           /** Codes for the active player and the rival */
  int socketfd;				                    /** Socket descriptor */
  struct sockaddr_in serverAddress;	      /** Server address structure */
  unsigned int port;			                /** Listening port */
  struct sockaddr_in player1Address;	    /** Client address structure for player 1 */
  struct sockaddr_in player2Address;	    /** Client address structure for player 2 */
  int socketPlayer1;			                /** Socket descriptor for player 1 */
  int socketPlayer2;			                /** Socket descriptor for player 2 */
  unsigned int clientLength;		          /** Length of client structure */
  tThreadArgs *threadArgs; 		            /** Thread parameters */
  pthread_t threadID;			                /** Thread ID */
  int msgLength;
  unsigned int points;

  // Seed
  srand (time (0));

  // Check arguments
  if (argc != 2)
  {
    fprintf (stderr,"ERROR wrong number of arguments\n");
    fprintf (stderr,"Usage:\n$>%s port\n", argv[0]);
    exit(1);
  }

  // Create the socket
  socketfd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketfd < 0)
    showError ("Error while creating the socket");

  // Init server structure
  memset (&serverAddress, 0, sizeof(serverAddress));

  // Get listening port
  port = atoi (argv[1]);

  // Fill server structure
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = htonl (INADDR_ANY);
  serverAddress.sin_port = htons (port);

  // Bind
  if (bind (socketfd,
    (struct sockaddr *) &serverAddress,
    sizeof (serverAddress)) < 0)
    showError ("ERROR while binding");

  /***************************************** 1 *****************************************/
  // Listen
  listen (socketfd, 2);

  // Get length of player 1 structure
  clientLength = sizeof (player1Address);

  // Accept
  socketPlayer1 = accept (socketfd, (struct sockaddr *) &player1Address, &clientLength);
  if (socketPlayer1 < 0)
    showError ("ERROR while accepting player 1");
  else
    fprintf(stdout, "Player 1 is connected!\n");

  // Get length of player 2 structure
  clientLength = sizeof (player2Address);

  // Accept
  socketPlayer2 = accept (socketfd, (struct sockaddr *) &player2Address, &clientLength);
  if (socketPlayer2 < 0)
    showError ("ERROR while accepting player 2");
  else
    fprintf (stdout, "Player 2 is connected!\n");

  /***************************************** 2 *****************************************/
  // Init and read player 1 name
  memset (session.player1Name, 0, STRING_LENGTH);
  msgLength = recv (socketPlayer1, session.player1Name, STRING_LENGTH - 1, 0);
  
  // Check read bytes
  if (msgLength < 0)
    showError ("ERROR while reading name of player 1");
  else
    printf ("Name of player 1 received: %s\n", session.player1Name);

  // Init and read player 2 name
  memset (session.player2Name, 0, STRING_LENGTH);
  msgLength = recv (socketPlayer2, session.player2Name, STRING_LENGTH - 1, 0);
  
  // Check read bytes
  if (msgLength < 0)
    showError ("ERROR while reading name of player 2");
  else
    printf ("Name of player 2 received: %s\n", session.player2Name);

  /***************************************** 3 *****************************************/
  // Initialize and print game session
  initSession (&session);
  printSession (&session);
  endOfGame = FALSE;
  currentPlayer = player1;

  /***************************************** 4 *****************************************/
  while (!endOfGame)
  {
    if (currentPlayer == player1)
    {
      manage_bet (socketPlayer1, &session.player1Stack, &session.player1Bet);
      manage_bet (socketPlayer2, &session.player2Stack, &session.player2Bet);
      turn (socketPlayer1, socketPlayer2, &session, &session.player1Deck);
      turn (socketPlayer2, socketPlayer1, &session, &session.player2Deck);
    }
    else
    {
      manage_bet (socketPlayer2, &session.player2Stack, &session.player2Bet);
      manage_bet (socketPlayer1, &session.player1Stack, &session.player1Bet);
      turn (socketPlayer2, socketPlayer1, &session, &session.player2Deck);
      turn (socketPlayer1, socketPlayer2, &session, &session.player1Deck);
    }

    updateStacks (&session);

    if (session.player1Stack == 0)
    {
      code = TURN_GAME_LOSE;
      msgLength = send (socketPlayer1, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player.");

      codeRival = TURN_GAME_WIN;
      msgLength = send (socketPlayer2, &codeRival, sizeof (codeRival), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player.");

      endOfGame = TRUE;

      // Close sockets
      close (socketPlayer1);
      close (socketPlayer2);
      close (socketfd);
    }
    else if (session.player2Stack == 0)
    {
      code = TURN_GAME_LOSE;
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player.");

      codeRival = TURN_GAME_WIN;
      msgLength = send (socketPlayer1, &codeRival, sizeof (codeRival), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player.");

      endOfGame = TRUE;

      // Close sockets
      close (socketPlayer1);
      close (socketPlayer2);
      close (socketfd);
    }
    else
    {
      currentPlayer = getNextPlayer (currentPlayer);
      clearDeck (&session.gameDeck);
      initDeck (&session.gameDeck);
      clearDeck (&session.player1Deck);
      clearDeck (&session.player2Deck);
      printSession (&session);
    }
  }
}
