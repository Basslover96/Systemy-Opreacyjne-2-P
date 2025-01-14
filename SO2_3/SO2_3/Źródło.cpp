#include <curses.h>
#include <thread>
#include <vector>
#include <ctime>
#include "Chicken.h"
#include "Player.h"
#include <string>
#include <mutex>
#include <atomic>

using namespace std;

int win_height, win_width;
int henhouse_height, henhouse_width;
int feeder_height, feeder_width;
WINDOW * henhouse;
WINDOW * feeder;
WINDOW * outside;
vector<Chicken> chickens;
Player player;
int number_of_chickens = 5;
int moves = 10;
int chickens_on_screen = 0;
vector<short> eat = { 0,0,0,0 };
mutex m;
mutex feeder_mutex;
mutex chicken_mutex;
mutex money_mutex;
condition_variable cv_feeder;
bool load_feed = false;
bool f_pressed;
int move_sign;
bool end_game = false;
float money = 1000;
float chicken_price = 100;
float feed_price = 20;
float sell_chicken_price = 200; \
atomic_bool buy = false;

vector<thread> chickens_threads;

void sellChickens();
void buyChicken();
void moveChicken(Chicken& chicken);

//Nape�nienie koryta pasz�.
void loadFeed(int troughNumber)
{
	if (eat[troughNumber] == 0) {
		eat[troughNumber] += 1;
		this_thread::sleep_for(std::chrono::milliseconds(500));
		eat[troughNumber] += 1;
		this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}
//G��wna funkcja karmienia.
void loadFeeder()
{
	while (!end_game && (chickens_on_screen > 0 || money>chicken_price)) {
		unique_lock<mutex> lk(feeder_mutex);
		while (!load_feed && !end_game)
		{
			cv_feeder.wait(lk);
		};
		load_feed = false;
		if (player.topY == 0) {
			if (player.leftX + 2 < 1 * 29)
			{
				loadFeed(0);
			}
			else
				if ((player.leftX + 2 > 1 * 29) && (player.leftX + 2 < 2 * 29)) {
					loadFeed(1);
				}
				else
					if ((player.leftX + 2 > 2 * 29) && (player.leftX + 2 < 3 * 29))
					{
						loadFeed(2);
					}
					else
						if (player.leftX + 2 > 3 * 29)
						{
							loadFeed(3);
						}
		}
		money_mutex.lock();
		money -= feed_price;
		money_mutex.unlock();
		cv_feeder.notify_one();
	}
}

float changePrice(float price) {
	return  (price*1.2 - price * 0.8) * ((((float)rand()) / (float)RAND_MAX)) + price * 0.8;
}

//Aktualizacja cen
void setPrices() {
	while ((chickens_on_screen > 0 || money>chicken_price) && !end_game) {
		this_thread::sleep_for(std::chrono::seconds(20));
		money_mutex.lock();
		chicken_price = changePrice(chicken_price);
		if (chicken_price < 70) chicken_price = 70;
		sell_chicken_price = changePrice(sell_chicken_price);
		if (sell_chicken_price < 160) sell_chicken_price = 160;
		feed_price = changePrice(feed_price);
		if (feed_price < 10) feed_price = 10;
		money_mutex.unlock();
	}
}

//Funkcje dotycz�ce gracza.
void generatePlayer()
{
	player.currentPosition = rand() % 5 + 1;
	player.leftX = rand() % 113;
	player.topY = 1;
	player.direction = rand() % 2;
}
void movePlayer()
{
	while ((chickens_on_screen > 0 || money>chicken_price) && !end_game)
	{
		unique_lock<mutex> lk(feeder_mutex);
		move_sign = getch();
		switch (move_sign)
		{
		case KEY_LEFT:
			player.direction = 0;
			if (player.leftX > 0)
				player.leftX -= 1;
			break;
		case KEY_RIGHT:
			player.direction = 1;
			if (player.leftX < 113)
				player.leftX += 1;
			break;
		case KEY_UP:
			if (player.topY > 0)
				player.topY -= 1;
			break;
		case KEY_DOWN:
			if (player.topY < 6)
				player.topY += 1;
			break;
		case 'f':
			if (((player.leftX + 2) != 1 * 29) && ((player.leftX + 2 != 2 * 29)) && ((player.leftX + 2) != 3 * 29) && (money>feed_price)) {
				f_pressed = true;
				move_sign = 32;
			}
			break;
		case 's': {
			chicken_mutex.lock();
			sellChickens();
			chicken_mutex.unlock();
		}
				  break;
		case 'b': {
			if (money > chicken_price) {
				chicken_mutex.lock();
				buyChicken();
				chicken_mutex.unlock();
			}
		}
				  break;
		case 'q': {
			end_game = true;
		}
				  break;
		default:
			break;
		}
		if (f_pressed) {
			{
				load_feed = true;
				f_pressed = false;
			}
			cv_feeder.notify_one();
			cv_feeder.wait(lk, [] { return !load_feed; });
		}
	}
}
void drawRightPlayer()
{
	mvwprintw(outside, player.topY, player.leftX + 1, "(*_*)/");
	mvwprintw(outside, player.topY + 1, player.leftX + 1, "<) )");
	mvwprintw(outside, player.topY + 2, player.leftX + 1, "/  \\");
}
void drawLeftPlayer()
{
	mvwprintw(outside, player.topY, player.leftX, "\\(*_*)");
	mvwprintw(outside, player.topY + 1, player.leftX + 1, "(  (>");
	mvwprintw(outside, player.topY + 2, player.leftX + 1, "/  \\");
}

//Rysowanie okien.
void drawOutside()
{
	init_pair(119, COLOR_WHITE, COLOR_BLACK);
	attron(COLOR_PAIR(119));
	wbkgd(outside, COLOR_PAIR(119));
	money_mutex.lock();
	mvwprintw(outside, 9, 5, "MONEY: %.1f          FEED PRICE: %.1f          BUY CHICKEN PRICE: %.1f          SELL CHICKEN PRICE: %.1f", money, feed_price, chicken_price, sell_chicken_price);
	money_mutex.unlock();
	attroff(COLOR_PAIR(119));
}
void drawHenhouse()
{
	init_pair(120, COLOR_WHITE, COLOR_GREEN);
	wattron(henhouse, COLOR_PAIR(120));
	box(henhouse, 0, 0);
	wbkgd(henhouse, COLOR_PAIR(120));
	wattroff(henhouse, COLOR_PAIR(120));
}
void drawFeeder()
{
	getmaxyx(feeder, feeder_height, feeder_width);
	init_pair(121, COLOR_WHITE, COLOR_YELLOW);
	wattron(feeder, COLOR_PAIR(121));
	box(feeder, 0, 0);
	wbkgd(feeder, COLOR_PAIR(121));
	wattroff(feeder, COLOR_PAIR(121));
	for (int i = 1; i < 4; i++)
	{
		wmove(feeder, 1, i * 29);
		wvline(feeder, 0, 2);
	}
	for (int i = 0; i<4; i++)
	{
		if (eat[i] == 1)
		{
			if (i < 3) {
				mvwprintw(feeder, 1, i * 29 + 1, "**************");
				mvwprintw(feeder, 2, i * 29 + 1, "**************");
			}
			else
			{
				mvwprintw(feeder, 1, i * 29 + 1, "**************");
				mvwprintw(feeder, 2, i * 29 + 1, "**************");
			}
		}
		else
			if (eat[i] == 2)
			{
				if (i < 3) {
					mvwprintw(feeder, 1, i * 29 + 1, "****************************");
					mvwprintw(feeder, 2, i * 29 + 1, "****************************");
				}
				else
				{
					mvwprintw(feeder, 1, i * 29 + 1, "*****************************");
					mvwprintw(feeder, 2, i * 29 + 1, "*****************************");
				}
			}
	}
};

//Funkcje dotycz�ce kurczak�w.
void generateChickens()
{
	getmaxyx(henhouse, henhouse_height, henhouse_width);
	for (int i = 0; i < 100; i++) {
		chickens.emplace_back(Chicken());
		chickens[i].id = i;
		chickens[i].direction = rand() % 2;
		chickens[i].event_type = 0;
		chickens[i].topY = rand() % (henhouse_height - 13) + 1;
		chickens[i].leftX = rand() % (henhouse_width - 11) + 1;
		chickens[i].food = rand() % 50 + 70;
		chickens[i].isHungry = false;
		chickens[i].color = 0;
		chickens[i].level = 1;
		if(i<number_of_chickens)
		chickens[i].isVisible = true;
		else
		chickens[i].isVisible = false;
	}
	chickens_on_screen = number_of_chickens;
}

void drawLeftChicken(Chicken chicken)
{
	mvwprintw(henhouse, chicken.topY, chicken.leftX + 2, "\\\\__");
	mvwprintw(henhouse, chicken.topY + 1, chicken.leftX + 1, "/.__.\\");
	mvwprintw(henhouse, chicken.topY + 2, chicken.leftX + 1, "\\ \\/ /");
	mvwprintw(henhouse, chicken.topY + 3, chicken.leftX + 1, "/    \\_'");
	mvwprintw(henhouse, chicken.topY + 4, chicken.leftX, "(     -/");
	mvwprintw(henhouse, chicken.topY + 5, chicken.leftX + 1, "\\____/");
	mvwprintw(henhouse, chicken.topY + 6, chicken.leftX + 2, "|  |");
	mvwprintw(henhouse, chicken.topY + 7, chicken.leftX + 2, "\"  \"");
	mvwprintw(henhouse, chicken.topY + 7, chicken.leftX + 3, "%d",chicken.food);
}
void drawRightChicken(Chicken chicken)
{
	mvwprintw(henhouse, chicken.topY, chicken.leftX + 4, "__//");
	mvwprintw(henhouse, chicken.topY + 1, chicken.leftX + 3, "/.__.\\");
	mvwprintw(henhouse, chicken.topY + 2, chicken.leftX + 3, "\\ \\/ /");
	mvwprintw(henhouse, chicken.topY + 3, chicken.leftX, "'__/    \\");
	mvwprintw(henhouse, chicken.topY + 4, chicken.leftX + 1, "\\-      )");
	mvwprintw(henhouse, chicken.topY + 5, chicken.leftX + 2, "\\_____/");
	mvwprintw(henhouse, chicken.topY + 6, chicken.leftX + 4, "|  |");
	mvwprintw(henhouse, chicken.topY + 7, chicken.leftX + 4, "\"  \"");
	mvwprintw(henhouse, chicken.topY + 7, chicken.leftX + 5, "%d", chicken.food);
}

void sellChickens() {
	for (int i = 0; i < number_of_chickens; i++) {
		if (chickens[i].level >= 3 && chickens[i].color == 3 && chickens[i].isVisible) {
			chickens[i].isVisible = false;
			chickens_on_screen--;
			money_mutex.lock();
			money += sell_chicken_price;
			money_mutex.unlock();
		}
	}
}
void buyChicken() {
	int index=-1;
	for (int i = 0; i < 100; i++) {
		if (!chickens[i].isVisible) {
			chickens[i].direction = rand() % 2;
			chickens[i].event_type = 0;
			chickens[i].topY = rand() % (henhouse_height - 13) + 1;
			chickens[i].leftX = rand() % (henhouse_width - 11) + 1;
			chickens[i].food = rand() % 50 + 70;
			chickens[i].isHungry = false;
			chickens[i].color = 0;
			chickens[i].level = 1;
			chickens[i].isVisible = true;
			chickens_on_screen++;
			number_of_chickens++;
			money_mutex.lock();
			money -= chicken_price;
			money_mutex.unlock();
			index = i;
			break;
		}
	}
	
	if(index!=-1)
	chickens_threads.emplace_back(thread(moveChicken, std::ref(chickens[index])));

}

short checkEvent(Chicken chicken)
{
	if (chicken.direction == 0)
	{
		if ((chicken.leftX == 1) && (chicken.topY == 1)) return 5;
		if ((chicken.leftX == 1) && (chicken.topY == (henhouse_height - 13))) return 7;
		if (chicken.leftX == 1) return 1;
	}
	else
	{
		if ((chicken.leftX == (henhouse_width - 11)) && (chicken.topY == 1)) return 6;
		if ((chicken.leftX == (henhouse_width - 11)) && (chicken.topY == (henhouse_height - 13))) return 8;
		if (chicken.leftX == (henhouse_width - 11)) return 2;
	}
	if (chicken.topY == 1) return 3;
	if (chicken.topY == (henhouse_height - 13)) return 4;
	return 0;
}

void tryToEat(Chicken& chicken, int localizationCorrection)
{
	if (chicken.leftX + localizationCorrection < 29)
	{
		if (eat[0] == 2)
		{
			this_thread::sleep_for(std::chrono::milliseconds(500));
			eat[0] -= 1;
			chicken.food += 30;
			this_thread::sleep_for(std::chrono::milliseconds(500));
			eat[0] -= 1;
			chicken.food += 30;
			chicken.isHungry = false;
			chicken.level += 1;
		}
	}
	else {
		if (chicken.leftX + localizationCorrection >= 29 && chicken.leftX + 4 < 58)
		{
			if (eat[1] == 2)
			{
				this_thread::sleep_for(std::chrono::milliseconds(500));
				eat[1] -= 1;
				chicken.food += 30;
				this_thread::sleep_for(std::chrono::milliseconds(500));
				eat[1] -= 1;
				chicken.food += 30;
				chicken.isHungry = false;
				chicken.level += 1;
			}
		}
		else
			if (chicken.leftX + localizationCorrection >= 58 && chicken.leftX + 4 < 87)
			{
				if (eat[2] == 2)
				{
					this_thread::sleep_for(std::chrono::milliseconds(500));
					eat[2] -= 1;
					chicken.food += 30;
					this_thread::sleep_for(std::chrono::milliseconds(500));
					eat[2] -= 1;
					chicken.food += 30;
					chicken.isHungry = false;
					chicken.level += 1;
				}
			}
			else
			{
				if (eat[3] == 2)
				{
					this_thread::sleep_for(std::chrono::milliseconds(500));
					eat[3] -= 1;
					chicken.food += 30;
					this_thread::sleep_for(std::chrono::milliseconds(500));
					eat[3] -= 1;
					chicken.food += 30;
					chicken.isHungry = false;
					chicken.level += 1;
				}
			}
	}
}
void moveChicken(Chicken& chicken)
{
	while (chicken.isVisible && !end_game) {
		if (chicken.food<40)
		{
			chicken.color = 1;
			chicken.isHungry = true;
			while (chicken.isHungry) {
				chicken.food--;
				if (chicken.food < 20 && chicken.color == 1)
				{
					chicken.color = 2;
				}
				if (chicken.topY < (henhouse_height - 13)) {
					chicken.topY += 1;
				}
				else
				{
					m.lock();
					if (chicken.direction == 0)
						tryToEat(chicken, 4);
					else
						tryToEat(chicken, 6);
					m.unlock();
				}
				if (chicken.food == 0) {
					chicken.isVisible = false;
					chickens_on_screen--;
				}
				this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}
		else
		{
			if(chicken.level>=3)
			{
				chicken.color = 3;
			}
			else
			{
				chicken.color = 0;
			}
			chicken.food--;
			chicken.event_type = checkEvent(chicken);
			switch (chicken.event_type)
			{
				//Normalny ruch
			case 0:
				if (chicken.direction == 0)
				{
					chicken.leftX -= rand() % 2;
					chicken.topY += rand() % 3 - 1;
				}
				else
				{
					chicken.leftX += rand() % 2;
					chicken.topY += rand() % 3 - 1;
				}
				break;
				//Lewa kraw�d�
			case 1:
				chicken.direction = 1;
				chicken.leftX += rand() % 2;
				chicken.topY += rand() % 3 - 1;
				break;
				//Prawa kraw�d�
			case 2:
				chicken.direction = 0;
				chicken.leftX -= rand() % 2;
				chicken.topY += rand() % 3 - 1;
				break;
				//G�rna kraw�d�
			case 3:
				chicken.direction == 0 ? chicken.leftX -= rand() % 2 : chicken.leftX += rand() % 2;
				chicken.topY += rand() % 2;
				break;
				//Dolna kraw�d�
			case 4:
				chicken.direction == 0 ? chicken.leftX -= rand() % 2 : chicken.leftX += rand() % 2;
				chicken.topY -= rand() % 2;
				break;
				//Lewy g�rny r�g
			case 5:
				chicken.direction = 1;
				chicken.leftX += rand() % 2;
				chicken.topY += rand() % 2;
				break;
				//Prawy g�rny r�g
			case 6:
				chicken.direction = 0;
				chicken.leftX -= rand() % 2;
				chicken.topY += rand() % 2;
				break;
				//Lewy dolny r�g
			case 7:
				chicken.direction = 1;
				chicken.leftX += rand() % 2;
				chicken.topY -= rand() % 2;
				break;
				//Prawy dolny r�g
			case 8:
				chicken.direction = 0;
				chicken.leftX -= rand() % 2;
				chicken.topY -= rand() % 2;
				break;
			}
			if (chicken.food == 0) {
				chicken.isVisible = false;
				chickens_on_screen--;
			}
			this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

}

//Zarz�dzanie oknami.
void clearAll()
{
	wclear(henhouse);
	wclear(feeder);
	wclear(outside);
}
void drawAll()
{
	drawHenhouse();
	wrefresh(henhouse);
	drawFeeder();
	wrefresh(feeder);
	drawOutside();
	wrefresh(outside);
}
void refreshAll()
{
	wrefresh(henhouse);
	wrefresh(feeder);
	wrefresh(outside);
}

//G��wna funkcja rysuj�ca.
void draw()
{
	while ((chickens_on_screen>0 || money>chicken_price) && !end_game) {

		clearAll();
		drawAll();
		chicken_mutex.lock();
		int color_id;
		for (int i=0; i<100;i++)
		{
			if (chickens[i].isVisible) {
				switch (chickens[i].color)
				{
				case 1:
				{
					init_pair(122, COLOR_YELLOW, COLOR_GREEN);
					wattron(henhouse, COLOR_PAIR(122));
					color_id = 122;
				}
				break;
				case 2:
				{
					init_pair(123, COLOR_RED, COLOR_GREEN);
					wattron(henhouse, COLOR_PAIR(123));
					color_id = 123;
				}
				break;
				case 3:
				{
					init_pair(124, COLOR_BLUE, COLOR_GREEN);
					wattron(henhouse, COLOR_PAIR(124));
					color_id = 124;
				}
				break;
				default:
				{
					init_pair(125, COLOR_WHITE, COLOR_GREEN);
					wattron(henhouse, COLOR_PAIR(125));
					color_id = 125;
				}
				break;
				}
				//Rysowanie kurczaka.
				(chickens[i].direction == 0) ? drawLeftChicken(chickens[i]) : drawRightChicken(chickens[i]);
				wattroff(henhouse, COLOR_PAIR(color_id));
			}
		}
		chicken_mutex.unlock();
		(player.direction == 0) ? drawLeftPlayer() : drawRightPlayer();
		refreshAll();
		this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	clearAll();
	money = 0;
	chicken_price = 0;
	sell_chicken_price = 0;
	feed_price = 0;
	drawAll();
	refreshAll();
	mvwprintw(outside, 0, 13, "  /$$$$$$   /$$$$$$  /$$      /$$ /$$$$$$$$        /$$$$$$  /$$    /$$ /$$$$$$$$ /$$$$$$$  /$$");
	mvwprintw(outside, 1, 13, " /$$__  $$ /$$__  $$| $$$    /$$$| $$_____/       /$$__  $$| $$   | $$| $$_____/| $$__  $$| $$");
	mvwprintw(outside, 2, 13, "| $$  \\__/| $$  \\ $$| $$$$  /$$$$| $$            | $$  \\ $$| $$   | $$| $$      | $$  \\ $$| $$");
	mvwprintw(outside, 3, 13, "| $$ /$$$$| $$$$$$$$| $$ $$/$$ $$| $$$$$         | $$  | $$|  $$ / $$/| $$$$$   | $$$$$$$/| $$");
	mvwprintw(outside, 4, 13, "| $$|_  $$| $$__  $$| $$  $$$| $$| $$__/         | $$  | $$ \\  $$ $$/ | $$__/   | $$__  $$|__/");
	mvwprintw(outside, 5, 13, "| $$  \\ $$| $$  | $$| $$\\  $ | $$| $$            | $$  | $$  \\  $$$/  | $$      | $$  \\ $$");
	mvwprintw(outside, 6, 13, "|  $$$$$$/| $$  | $$| $$ \\/  | $$| $$$$$$$$      |  $$$$$$/   \\  $/   | $$$$$$$$| $$  | $$ /$$");
	mvwprintw(outside, 7, 13, " \\______/ |__/  |__/|__/     |__/|________/       \\______/     \\_/    |________/|__/  |__/|__/");
	refreshAll();
}

int main(int argc, char ** argv)
{
	srand(time(nullptr));

	initscr();
	curs_set(0);
	keypad(stdscr, true);
	start_color();
	noecho();

	//Stworzenie okien.
	getmaxyx(stdscr, win_height, win_width);
	henhouse = newwin(win_height - 10, win_width, 0, 0);
	feeder = newwin(4, win_width - 2, win_height - 15, 1);
	outside = newwin(10, win_width, win_height - 10, 0);

	//Generowanie kurczak�w i gracza.
	generatePlayer();
	generateChickens();

	chickens_threads.reserve(100);

	//Tworzenie w�tk�w.
	thread draw_thread(draw);
	thread player(movePlayer);
	thread feeder(loadFeeder);
	thread setAllPrices(setPrices);
	for (int i = 0;i<chickens_on_screen;i++)
	{
		this_thread::sleep_for(std::chrono::milliseconds(500));
		chickens_threads.emplace_back(thread(moveChicken, std::ref(chickens[i])));
	}

	//Ko�czenie pracy w�tk�w.
	for (std::thread & chicken_thread : chickens_threads)
	{
		if (chicken_thread.joinable())
			chicken_thread.join();
	}
	setAllPrices.join();
	feeder.join();
	player.join();
	draw_thread.join();

	keypad(stdscr, false);
	clear();
	endwin();
}