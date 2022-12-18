#define OLC_PGE_APPLICATION
#include <iostream>
#include <string_view>
#include <string>
#include "olcPixelGameEngine.h"

#define CARD_WIDTH 80
#define CARD_HEIGHT 116
#define CARD_PADDING 10
enum Suit { hearts = 0, diamonds = 1, clubs = 2, spades = 3, notrump = 4 };
enum Vul { both = 0, none = 1, ns = 2, ew = 3 };

struct Card {
	std::string direction;
	Suit suit;
	int spot;
	int scoreEW;
	int scoreNS;
};

struct Holding {
	Suit suit;
	int spots[13];
};

struct Hand {
	Holding hearts;
	Holding diamonds;
	Holding clubs;
	Holding spades;
};

struct Bid {
	std::string seat;
	std::string call;
	std::string note;
	bool alerted = false;
	Suit suit;
	int spot;
};

static std::map<char, int> CARD_TO_SPOT = {
	{ '2', 2 },
	{ '3', 3 },
	{ '4', 4 },
	{ '5', 5 },
	{ '6', 6 },
	{ '7', 7 },
	{ '8', 8 },
	{ '9', 9 },
	{ 'T', 10 },
	{ 'J', 11 },
	{ 'Q', 12 },
	{ 'K', 13 },
	{ 'A', 14 }
};

template <typename T>
class Cycle {
public: 
	explicit Cycle(const std::vector<T> &vect) : m_vect(vect) {
		m_current_item = m_vect.cbegin();
	}
	explicit Cycle(std::initializer_list<T> c) : m_vect(c) {
		m_current_item = m_vect.cbegin();
	}
	// return current and don't advance cycle
	T current() {
		return *m_current_item;
	}
	// return current and advance cycle
	T next() {
		T val = *m_current_item;
		m_current_item++;
		if (m_current_item >= m_vect.cend()) {
			m_current_item = m_vect.cbegin();
		}
		return val;
	}
	void back() {
		if (m_current_item <= m_vect.cbegin()) {
			m_current_item = m_vect.cend();
		}
		m_current_item--;
	}
private:
	const std::vector<T> m_vect;
	typename std::vector<T>::const_iterator m_current_item;
};

class Lin : public olc::PixelGameEngine
{
public:
	Lin()
	{
		sAppName = "Lin Viewer";
	}
private:
	olc::Sprite cardsSprite;
	std::string linData;
	std::string boardName;
	Vul vulnerability;
	std::string north, south, east, west;
	std::string dealer;
	std::string declarer;
	std::string contract;
	Suit trumpSuit;
	int claim = -1;
	int tricksClaimed = 0;
	bool advancing = false;
	bool regressing = false;
	std::vector<Bid*> auction;
	std::vector<Card*> play;
	int current_card = 0;
	int nsTricks = 0;
	int ewTricks = 0;
	double desiredFrameRate = 25;
	std::map<std::string, Hand> hands = {
		{"south", Hand()},
		{"north", Hand()},
		{"east", Hand()},
		{"west", Hand()}
	};
	std::vector<Suit> suits = { Suit::spades, Suit::hearts, Suit::diamonds, Suit::clubs };
	Cycle<std::string> seats{"north", "east", "south", "west"};
	Cycle<std::string> playOrder{ "north", "east", "south", "west" };

public:
	bool OnUserCreate() override
	{
		// Called once at the start, so create things here
		cardsSprite = olc::Sprite("./assets/cards.png");
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		regressing = advancing = false;
		std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
		Clear(olc::DARK_GREEN);
		SetPixelMode(olc::Pixel::MASK);
		DrawNorth();
		DrawSouth();
		DrawEast();
		DrawWest();
		DrawVul(40,20);
		DrawScore(1060,20);
    if (GetKey(olc::Key::W).bPressed) {
			advancing = true;
			current_card++;
			if (current_card >= play.size() && claim == -1) current_card = play.size() - 1;
			if (current_card >= claim && claim != -1) current_card = claim;
		}
    if (GetKey(olc::Key::S).bPressed) {
			regressing = true;
			current_card--;
			if (current_card < 0) current_card = 0;
		}
		if (regressing) {
			if (current_card < play.size()) {
				Card *previousCard = play[current_card];
				nsTricks -= previousCard->scoreNS;
				ewTricks -= previousCard->scoreEW;
				Holding& holding = HoldingForCard(previousCard);
				holding.spots[previousCard->spot - 2] = previousCard->spot;
			}
		}
		if (advancing) {
			if (current_card <= play.size()) {
				Card *currentCard = play[current_card - 1];
				nsTricks += currentCard->scoreNS;
				ewTricks += currentCard->scoreEW;
				Holding& holding = HoldingForCard(currentCard);
				holding.spots[currentCard->spot - 2] = 0;
			}
		}
		if (current_card == 0) {
			DrawBidding();
		} else if (current_card == claim) {
			DrawClaim();
		} else {
			std::vector<Card *> trick;
			int min = (current_card - 1) - ((current_card-1) % 4);
			for (int i = min; i < current_card; i++) {
				trick.push_back(play[i]);
				DrawTrick(trick);
			}
		}
		SetPixelMode(olc::Pixel::NORMAL);
		std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
		std::chrono::duration<double, std::milli> actualFrameTime = end - start;
		double desiredFrameTime = 1.0 / desiredFrameRate * 1000.0;
		if (actualFrameTime.count() < desiredFrameTime) {
			std::chrono::duration<double, std::milli> sleep_time(desiredFrameTime - actualFrameTime.count());
			auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time);
      std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
		}
		return true;
	}

	void SetLinData(const std::string& data) {
		linData = data;
		ParseLinData();
	}

private:
	void DrawClaim() {
		int x = 340;
		int y = 157;
		int w = 520;
		int h = 483;
		FillRect(x-20,y-20,w + 40, h + 42,olc::BLUE);
		FillRect(x, y, w, h, olc::BLACK);
		std::stringstream str;
		str << tricksClaimed << " tricks claimed.";
		DrawStringInBox(x,y+h/2,w, str.str(), olc::WHITE, 2);
	}
	void DrawStringInBox(int x, int y, int w, const std::string& text,
		 const olc::Pixel& color, int scale = 1) {
		 olc::vi2d size = GetTextSizeProp(text) * scale;
		 int xOff = std::max(w - size.x, 2) / 2;
		 DrawStringProp(x + xOff, y, text, color, scale);
	}
	void DrawBidding() {
		int x = 340;
		int y = 157;
		int w = 520;
		int h = 483;
		int bw = w / 4;
		int bh = h / 8;
		FillRect(x-20,y-20,w + 40, h + 42,olc::BLUE);
		FillRect(x, y, w, h, olc::BLACK);
		olc::Pixel blockColor = olc::RED;
		olc::Pixel textColor = olc::WHITE;
		// EW headers
    if ( vulnerability == Vul::ns || vulnerability == Vul::none ) {
			blockColor = olc::WHITE;
			textColor = olc::BLACK;
		}
		FillRect(x + 2, y + 2, bw - 4, bh - 4, blockColor);
		DrawStringInBox(x + 2, y + 8, bw - 4, "West", textColor, 2);
		DrawStringInBox(x + 2, y + 28, bw - 4, west, textColor, 1);
		FillRect(x + bw * 2 + 2, y + 2, bw - 4, bh - 4, blockColor);
		DrawStringInBox(x + bw * 2 + 2, y + 8, bw - 4, "East", textColor, 2);
		DrawStringInBox(x + bw * 2 + 2, y + 28, bw - 4, east, textColor, 1);
		// NS headers
		blockColor = olc::RED;
		textColor = olc::WHITE;
    if ( vulnerability == Vul::ew || vulnerability == Vul::none ) {
			blockColor = olc::WHITE;
			textColor = olc::BLACK;
		}
		FillRect(x + bw + 2, y + 2, bw - 4, bh - 4, blockColor);
		DrawStringInBox(x + bw + 2, y + 8, bw - 4, "North", textColor, 2);
		DrawStringInBox(x + bw + 2, y + 28, bw - 4, north, textColor, 1);
		FillRect(x + bw * 3 + 2, y + 2, bw - 4, bh - 4, blockColor);
		DrawStringInBox(x + bw * 3 + 2, y + 8, bw - 4, "South", textColor, 2);
		DrawStringInBox(x + bw * 3 + 2, y + 28, bw - 4, south, textColor, 1);

	  Cycle<std::string> order{"west", "north", "east", "south"};
		int row = 1;
		int col = 0;
		olc::Pixel bgColor = olc::Pixel(173, 216, 230); // light blue
		for ( auto bid : auction ) {
			while (order.next() != bid->seat ) {
				FillRect(x + col * bw + 2, y + row * bh + 2, bw - 4, bh - 4, bgColor);
				col += 1;
			}
			FillRect(x + col * bw + 2, y + row * bh + 2, bw - 4, bh - 4, bgColor);
			DrawStringInBox(x + col * bw + 2, y + row * bh + 2 + bh / 3, bw - 4, bid->call, olc::BLACK, 2);
			col += 1;
			if ( col >= 4 ) {
				col = 0;
				row += 1;
			}
		}
	}
	void DrawTrick(const std::vector<Card*>& cards) {
		FillRect(320,137,560,525,olc::BLUE);
	  for (auto &card : cards) {
			int x, y, rot = olc::Sprite::NONE;
			if (card->direction == "north") {
				x = (ScreenWidth() / 2) - (CARD_WIDTH / 2);
				y = (ScreenHeight() / 2) - (CARD_HEIGHT - 10);
			} else if (card->direction == "east") {
				x = (ScreenWidth() / 2);
				y = (ScreenHeight() / 2) - (CARD_HEIGHT / 2);
			} else if (card->direction == "south") {
				x = (ScreenWidth() / 2) - (CARD_WIDTH / 2);
				y = (ScreenHeight() / 2) - 10;
			} else if (card->direction == "west") {
				x = (ScreenWidth() / 2) - CARD_WIDTH;
				y = (ScreenHeight() / 2) - (CARD_HEIGHT / 2);
			}
			DrawCard(*card, x, y, rot);
		}
	}
	void DrawScore(int x, int y) {
     FillRect(x, y, 100, 100, olc::BLACK);
		 FillRect(x+2, y+2, 36, 96, olc::GREY);
		 FillRect(x+41, y+61, 57, 37, olc::GREY);
		 FillRect(x+41, y+2, 57, 57, olc::DARK_YELLOW);

		 // Contract and declarer
		 DrawStringInBox(x + 41, y + 10, 57, contract, olc::BLACK, 2);
		 DrawStringInBox(x + 41, y + 35, 57, declarer, olc::BLACK, 1);

		 // N/S tricks
		 DrawStringInBox(x + 2, y + 48, 36, std::to_string(nsTricks), olc::BLACK, 1);
		 DrawStringInBox(x + 43, y + 77, 57, std::to_string(ewTricks), olc::BLACK, 1);
	}

	void DrawVul(int x, int y) {
    // Outline
		FillRect(x-2,y-2,105,105,olc::BLACK);
		FillRect(x-1,y-1,103,103,olc::BLACK);

		// Board Title
		DrawStringInBox(x + 20, y + 48, 60, boardName, olc::WHITE, 1);

		// North/South
		olc::Pixel color = olc::WHITE;
		olc::Pixel textColor = olc::BLACK;
	  if (vulnerability == Vul::ns || vulnerability == Vul::both) {
			color = olc::RED;
			textColor = olc::WHITE;
	  }
		//North
	  FillRect(x + 20, y, 60, 21, color);
	  FillTriangle(x, y, x + 20, y, x + 20, y + 20, color);
	  FillTriangle(x + 80, y, x + 80, y + 20, x + 100, y, color);
		if ( dealer == "north" ) {
			DrawString(x + 47, y + 7, "D", textColor);
		}

		//South
	  FillRect(x + 20, y + 80, 60, 21, color);
	  FillTriangle(x, y + 100, x + 20, y + 80, x + 20, y + 100, color);
	  FillTriangle(x + 100, y + 100, x + 80, y + 80, x + 80, y + 100, color);
		if ( dealer == "south" ) {
			DrawString(x + 47, y + 88, "D", textColor);
		}

		// East/West
		color = olc::WHITE;
		textColor = olc::BLACK;
	  if (vulnerability == Vul::ew || vulnerability == Vul::both) {
			color = olc::RED;
			textColor = olc::WHITE;
	  }
		// West
	  FillRect(x, y + 20, 21, 60, color);
	  FillTriangle(x, y, x + 20, y + 20, x, y + 20, color);
	  FillTriangle(x, y + 100, x + 20, y + 80, x, y + 80, color);
		if ( dealer == "west" ) {
			DrawString(x + 6, y + 48, "D", textColor);
		}
		//East
	  FillRect(x + 80, y + 20, 21, 60, color);
	  FillTriangle(x + 100, y, x + 100, y + 20, x + 80, y + 20, color);
	  FillTriangle(x + 100, y + 100, x + 100, y + 80, x + 80, y + 80, color);
		if ( dealer == "east" ) {
			DrawString(x + 88, y + 48, "D", textColor);
		}
	}
	void DrawSouth() {
		DrawHorizontalHand(ScreenHeight() - 10 - CARD_HEIGHT, hands.at("south"));
	}
	void DrawEast() {
		DrawVerticalHand(ScreenWidth() - (30 * (8 + 2)), hands.at("east"));
	}
	void DrawWest() {
		DrawVerticalHand(10, hands.at("west"));
	}
	void DrawNorth() {
		DrawHorizontalHand(10, hands.at("north"));
	}
	Holding& HoldingForCard(Card* card) {
		Hand& hand = hands[card->direction];
		return HoldingForSuit(hand, card->suit);
	}
	Holding& HoldingForSuit(Hand& hand, Suit& suit) {
		switch (suit) {
			case Suit::spades:
				return hand.spades;
			case Suit::hearts:
				return hand.hearts;
			case Suit::diamonds:
				return hand.diamonds;
			case Suit::clubs:
				return hand.clubs;
		}
		// Should never get here
		return hand.spades;
	}

	void DrawVerticalHand(int x, Hand& hand) {
		int original_x = x;
		int x_offset = 30;
		int y = (ScreenHeight() - (CARD_HEIGHT + 10) * 4) / 2;
		for (Suit s : suits) {
			Holding& holding = HoldingForSuit(hand, s);
			for (int idx = 12; idx >= 0; idx--) {
				if (holding.spots[idx] != 0) {
					DrawCard(s, holding.spots[idx], x, y);
					x += x_offset;
				}
			}
			x = original_x;
			y += (CARD_HEIGHT + 10);
		}
	}

	void DrawHorizontalHand(int y, Hand& hand) {
		int x_offset = 30;
		int width = 15 * x_offset;
		int x = (ScreenWidth() - width) / 2;
		for (Suit s : suits) {
			Holding& holding = HoldingForSuit(hand, s);
			for (int idx = 12; idx >= 0; idx--) {
				if (holding.spots[idx] != 0) {
					DrawCard(s, holding.spots[idx], x, y);
					x += x_offset;
				}
			}
		}
	}

	void ParseLinData() {
		std::stringstream tokenizer(linData);
		std::string command;
		std::string data;
		int numPasses = 0;
		Bid *lastCall;
		bool bidIsACall = false;
		while (!tokenizer.eof()) {
			std::getline(tokenizer, command, '|');
			std::getline(tokenizer, data, '|');
			std::stringstream dataTok(data);
			if (command == "pn") {
				std::getline(dataTok, north, ',');
				std::getline(dataTok, south, ',');
				std::getline(dataTok, east, ',');
				std::getline(dataTok, west, ',');
			} else if (command == "mc") {
				claim = play.size() + 1;
				tricksClaimed = stoi(data);
			} else if (command == "pc") {
				Card *playedCard = new Card();
				playedCard->direction = playOrder.next();
				playedCard->spot = CARD_TO_SPOT.at(data[1]);
				switch(data[0]) {
				case 'N':
					playedCard->suit = Suit::notrump;
					break;
				case 'S':
					playedCard->suit = Suit::spades;
					break;
				case 'H':
					playedCard->suit = Suit::hearts;
					break;
				case 'D':
					playedCard->suit = Suit::diamonds;
					break;
				case 'C':
					playedCard->suit = Suit::clubs;
					break;
				}
				play.push_back(playedCard);
				if (play.size() % 4 == 0) {
					// trick is over, figure out who won it and reset playOrder accordingly
					Card* winner = play[play.size() - 4];
					for (int ind = play.size() - 3; ind < play.size(); ++ind) {
						if (
								(play[ind]->suit == winner->suit && play[ind]->spot > winner->spot) ||
							 	(winner->suit != trumpSuit && play[ind]->suit == trumpSuit)
						) {
							winner = play[ind];
						}
					}
					if (winner->direction == "north" || winner->direction == "south") {
						playedCard->scoreNS = 1;
					} else {
						playedCard->scoreEW = 1;
					}
					//std::cout << "TRICK OVER, WINNER = DIR: " << winner->direction << " SUIT: " << winner->suit << " SPOT: " << winner->spot << std::endl;
					//std::cout << "PLAY ORDER CURRENT: " << playOrder.current() << std::endl;
					while(playOrder.next() != winner->direction) {}
					//std::cout << "PLAY ORDER NEW + 1: " << playOrder.current() << std::endl;
					playOrder.back();
					//std::cout << "PLAY ORDER NEW: " << playOrder.current() << std::endl;
				}
			} else if (command == "an") {
				auction.back()->note = data;
			} else if (command == "mb") {
				Bid *b = new Bid();
				b->seat = seats.next();
				b->alerted = data.ends_with('!');
				//std::cout << "PARSING BID " << data <<  " by " << b->seat << " alerted? " << b->alerted << std::endl;
				bidIsACall = false;
				if (data == "p" || data == "P") {
					b->call = "P";
				} else if (data == "D") {
					b->call = "X";
				} else if (data == "R") {
					b->call = "XX";
				} else {
					bidIsACall = true;
					b->call = data;
					b->spot = std::stoi(data.substr(0,1));
          switch(data[1]) {
					case 'N':
						b->suit = Suit::notrump;
						break;
					case 'S':
						b->suit = Suit::spades;
						break;
					case 'H':
						b->suit = Suit::hearts;
						break;
					case 'D':
						b->suit = Suit::diamonds;
						break;
					case 'C':
						b->suit = Suit::clubs;
						break;
					}
				}
				auction.push_back(b);
				if (bidIsACall) {
					lastCall = b;
				}
				if (b->call == "P") {
					numPasses++;
					if (numPasses >= 3) {
						trumpSuit = lastCall->suit;
						contract = lastCall->call;
						declarer = lastCall->seat;
						auto same_suit = [lastCall](const Bid* bid){ return bid != lastCall && bid->suit == lastCall->suit; };
						auto result = std::find_if(++auction.crbegin(), auction.crend(), same_suit);
						if (result != auction.crend()) {
							declarer = (*result)->seat;
						}
						while (playOrder.next() != declarer) { }
						//std::cout << "LAST CALL " << lastCall->call << std::endl;
						//std::cout << "FINAL CONTRACT " << contract << std::endl;
						//std::cout << "FINAL DECLARER " << declarer << std::endl;
						//std::cout << "FIRST TO PLAY A CARD " << playOrder.current() << std::endl;
					}
				} else {
					numPasses = 0;
				}
			} else if (command == "sv") {
				if (data == "n") {
					vulnerability = Vul::ns;
				} else if (data == "e") {
					vulnerability = Vul::ew;
				} else if (data == "o") {
					vulnerability = Vul::none;
				} else if (data == "b") {
					vulnerability = Vul::both;
				}
			} else if (command == "ah") {
				boardName = data;
			} else if (command == "md") {
				int dn = std::stoi(data.substr(0,1));
				if (dn == 1) {
					dealer = "south";
					// advance seats to 'south';
					seats.next();
					seats.next();
				} else if (dn == 2) {
					dealer = "west";
					// advance seats to 'west'
					seats.next();
					seats.next();
					seats.next();
				} else if (dn == 3) {
					dealer = "north";
					// no need to advance seats
				} else {
					dealer = "east";
					// advance seats to 'east'
					seats.next();
				}
				//std::cout << "DEALER IS " << dealer << " next seat is " << seats.current() << std::endl;
				dataTok = std::stringstream(data.substr(1));
				Hand &eastHand = hands.at("east");
				eastHand.hearts = Holding { Suit::hearts, { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 } };
				eastHand.diamonds = Holding { Suit::diamonds, { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 } };
				eastHand.clubs = Holding { Suit::clubs, { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 } };
				eastHand.spades = Holding { Suit::spades, { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 } };
				std::vector<std::string> order = { "south", "west", "north", "east" };
				for(int i = 0; i < 3; ++i) {
					std::string handStr;
					std::getline(dataTok, handStr, ',');
					Hand &hand = hands.at(order[i]);
					hand.hearts = Holding { Suit::hearts, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
					hand.diamonds = Holding { Suit::diamonds, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
					hand.clubs = Holding { Suit::clubs, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
					hand.spades = Holding { Suit::spades, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
					Holding *holding;
					Holding *eastHolding;
					for (auto &ch : handStr) {
						if (ch == 'S') {
							holding = &hand.spades;
							eastHolding = &eastHand.spades;
						} else if (ch == 'H') {
							holding = &hand.hearts;
							eastHolding = &eastHand.hearts;
						} else if (ch == 'C') {
							holding = &hand.clubs;
							eastHolding = &eastHand.clubs;
						} else if (ch == 'D') {
							holding = &hand.diamonds;
							eastHolding = &eastHand.diamonds;
					  } else {
              int spot = CARD_TO_SPOT.at(ch);
							int spotInd = spot - 2;
							holding->spots[spotInd] = spot;
							eastHolding->spots[spotInd] = 0;
						}
					}
				}
			}
		}
	}
	void DrawCard(const Card& card, int x, int y, int rot = 0) {
		DrawCard(card.suit, card.spot, x, y, rot);
	}
	void DrawCard(Suit suit, int spot, int x, int y, int rot = 0) {
		if ( suit > Suit::spades ) {
			return;
		}
		int sprite_y = (CARD_HEIGHT + CARD_PADDING * 2) * suit + CARD_PADDING;
		int sprite_x = (CARD_WIDTH + CARD_PADDING * 2) * (spot - 2) + CARD_PADDING;
		DrawPartialSprite(x, y, &cardsSprite, sprite_x, sprite_y, CARD_WIDTH, CARD_HEIGHT, 1, rot);
	}
};


int main(int argc, char * argv[])
{
	Lin viewer;
	if (viewer.Construct(1200,800,1,1,false,true)) {
		if (argc > 1) {
			viewer.SetLinData(argv[1]);
		}
		viewer.Start();
	}

	return 0;
}
