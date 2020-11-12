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
    // a. Send TURN_BET and stack to player 1
    code = TURN_BET;
    msgLength = send (socketPlayer1, &code, sizeof(code), 0);
    if (msgLength < 0)
      showError ("ERROR while writing code to player.");

    code = session.player1Stack;
    msgLength = send (socketPlayer1, &code, sizeof(code), 0);
    if (msgLength < 0)
      showError ("ERROR while writing stack to player.");

    // b. Player 1 bet
    while (code != TURN_BET_OK)
    {
      // Read
      msgLength = recv (socketPlayer1, &session.player1Bet, sizeof(session.player1Bet), 0);
      if (msgLength < 0)
        showError ("ERROR while receiving player's bet.");

      if (session.player1Bet > 0 && session.player1Bet <= session.player1Stack 
        && session.player1Bet < MAX_BET)
        code = TURN_BET_OK;
      else
        code = TURN_BET;

      // Send
      msgLength = send (socketPlayer1, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player.");
    }

    // c. Send TURN_BET and stack to player 2
    code = TURN_BET;
    msgLength = send (socketPlayer2, &code, sizeof(code), 0);
    if (msgLength < 0)
      showError ("ERROR while writing code to player.");

    code = session.player2Stack;
    msgLength = send (socketPlayer2, &code, sizeof(code), 0);
    if (msgLength < 0)
      showError ("ERROR while writing stack to player.");

    while (code != TURN_BET_OK)
    {
      // Read
      msgLength = recv (socketPlayer2, &session.player2Bet, sizeof(session.player2Bet), 0);
      if (msgLength < 0)
        showError ("ERROR while receiving player's bet.");

      if (session.player2Bet > 0 && session.player2Bet <= session.player2Stack 
	  && session.player2Bet <= MAX_BET)
        code = TURN_BET_OK;
      else
        code = TURN_BET;

      // Send
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player.");
    }
    
    // d. Start game
    if (currentPlayer == player1)
    {
      // Get player 1 deck
      for (int i = 0; i < 2; i++)
      {
        session.player1Deck.cards[i] = getRandomCard (&session.gameDeck);
        session.player1Deck.numCards++;
      }
      // i. ii.
      // Send TURN_PLAY to player 1 
      code = TURN_PLAY;
      msgLength = send (socketPlayer1, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player 1.");

      // Send TURN_PLAY_WAIT to player 2
      code = TURN_PLAY_WAIT;
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player 2.");

      // Send deck to both players
      tDeck deck = session.player1Deck;
      msgLength = send (socketPlayer1, &deck, sizeof (deck), 0);
      if (msgLength < 0)
        showError ("ERROR while sending deck to player 1.");
      msgLength = send (socketPlayer2, &deck, sizeof (deck), 0);
      if (msgLength < 0)
        showError ("ERROR while sending deck to player 2.");

      // Get player 1 points
      points = calculatePoints (&session.player1Deck);

      // Send points to both players
      msgLength = send (socketPlayer1, &points, sizeof (points), 0);
      if (msgLength < 0)
        showError ("ERROR while sending points to player 1.");
      msgLength = send (socketPlayer2, &points, sizeof (points), 0);
      if (msgLength < 0)
        showError ("ERROR while sending points to player 2.");

      // Answer from player 1 choice: HIT or STAND
      msgLength = recv (socketPlayer1, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while receiving player 1 option.");

      // iii.
      while (code == TURN_PLAY_HIT)
      {
        session.player1Deck.cards[session.player1Deck.numCards++] = getRandomCard (&session.gameDeck);
        points = calculatePoints (&session.player1Deck);
        deck = session.player1Deck;
        
        // Check player points
        if (points > 21)
        {
          code = TURN_PLAY_OUT;
          msgLength = send (socketPlayer1, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while writing code to player.");
        }
        else
        {
          code = TURN_PLAY;
          msgLength = send (socketPlayer1, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while writing code to player.");

          code = TURN_PLAY_WAIT;
          msgLength = send (socketPlayer2, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while writing code to player.");
        }

        // Send deck to both players
        msgLength = send (socketPlayer1, &deck, sizeof (deck), 0);
        if (msgLength < 0)
          showError ("ERROR while sending deck to player 1.");
        msgLength = send (socketPlayer2, &deck, sizeof (deck), 0);
        if (msgLength < 0)
          showError ("ERROR while sending deck to player 2.");

        // Send points to both players
        msgLength = send (socketPlayer1, &points, sizeof (points), 0);
        if (msgLength < 0)
          showError ("ERROR while sending points to player 1.");
        msgLength = send (socketPlayer2, &points, sizeof (points), 0);
        if (msgLength < 0)
          showError ("ERROR while sending points to player 2.");

        if (!(points > 21))
        {
          // Answer from player 1 choice: HIT or STAND
          msgLength = recv (socketPlayer1, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while receiving player 1 option.");
        }
      }

      code = TURN_PLAY_WAIT;
      msgLength = send (socketPlayer1, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player 1.");

      code = TURN_PLAY_RIVAL_DONE;
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player 2.");

      currentPlayer = getNextPlayer (currentPlayer);
    }

    if (currentPlayer == player2)
    {
      // Get player 2 deck
      for (int i = 0; i < 2; i++)
      {
        session.player2Deck.cards[i] = getRandomCard (&session.gameDeck);
        session.player2Deck.numCards++;
      }
      // i. ii.
      // Send TURN_PLAY to player 2 
      code = TURN_PLAY;
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player 2.");

      // Send deck to both players
      tDeck deck = session.player2Deck;
      msgLength = send (socketPlayer1, &deck, sizeof (deck), 0);
      if (msgLength < 0)
        showError ("ERROR while sending deck to player 1.");
      msgLength = send (socketPlayer2, &deck, sizeof (deck), 0);
      if (msgLength < 0)
        showError ("ERROR while sending deck to player 2.");

      // Get player 1 points
      points = calculatePoints (&session.player2Deck);

      // Send points to both players
      msgLength = send (socketPlayer1, &points, sizeof (points), 0);
      if (msgLength < 0)
        showError ("ERROR while sending points to player 1.");
      msgLength = send (socketPlayer2, &points, sizeof (points), 0);
      if (msgLength < 0)
        showError ("ERROR while sending points to player 2.");

      // Answer from player 2 choice: HIT or STAND
      msgLength = recv (socketPlayer2, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while receiving player 2 option.");

      // iii.
      while (code == TURN_PLAY_HIT)
      {
        session.player2Deck.cards[session.player2Deck.numCards++] = getRandomCard (&session.gameDeck);
        points = calculatePoints (&session.player2Deck);
        deck = session.player2Deck;
        
        // Check player points
        if (points > 21)
        {
          code = TURN_PLAY_OUT;
          msgLength = send (socketPlayer2, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while writing code to player.");
        }
        else
        {
          code = TURN_PLAY;
          msgLength = send (socketPlayer2, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while writing code to player.");

          code = TURN_PLAY_WAIT;
          msgLength = send (socketPlayer1, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while writing code to player.");
        }

        // Send deck to both players
        msgLength = send (socketPlayer1, &deck, sizeof (deck), 0);
        if (msgLength < 0)
          showError ("ERROR while sending deck to player 1.");
        msgLength = send (socketPlayer2, &deck, sizeof (deck), 0);
        if (msgLength < 0)
          showError ("ERROR while sending deck to player 2.");

        // Send points to both players
        msgLength = send (socketPlayer1, &points, sizeof (points), 0);
        if (msgLength < 0)
          showError ("ERROR while sending points to player 1.");
        msgLength = send (socketPlayer2, &points, sizeof (points), 0);
        if (msgLength < 0)
          showError ("ERROR while sending points to player 2.");

        if (!(points > 21))
        {
          // Answer from player 1 choice: HIT or STAND
          msgLength = recv (socketPlayer2, &code, sizeof (code), 0);
          if (msgLength < 0)
            showError ("ERROR while receiving player 1 option.");
        }
      }

      code = TURN_PLAY_WAIT;
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player 2.");

      code = TURN_PLAY_RIVAL_DONE;
      msgLength = send (socketPlayer1, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player 1.");
    }

    updateStacks (&session);

    if (session.player1Stack == 0)
    {
      code = TURN_GAME_LOSE;
      msgLength = send (socketPlayer1, &code, sizeof (code), 0);
      if (msgLength < 0)
        showError ("ERROR while writing code to player.");
      
      code = TURN_GAME_WIN;
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
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

      code = TURN_GAME_WIN;
      msgLength = send (socketPlayer2, &code, sizeof (code), 0);
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
    }
  }
}
