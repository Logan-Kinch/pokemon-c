#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#include "io.h"
#include "character.h"
#include "poke327.h"
#include "pokemon.h"

typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

void io_init_terminal(void)
{
  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  start_color();
  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void io_reset_terminal(void)
{
  endwin();

  while (io_head) {
    io_tail = io_head;
    io_head = io_head->next;
    free(io_tail);
  }
  io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x)
{
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    if (io_head) {
      attron(COLOR_PAIR(COLOR_CYAN));
      mvprintw(y, x + 70, "%10s", " --more-- ");
      attroff(COLOR_PAIR(COLOR_CYAN));
      refresh();
      getch();
    }
    free(io_tail);
  }
  io_tail = NULL;
}

/**************************************************************************
 * Compares trainer distances from the PC according to the rival distance *
 * map.  This gives the approximate distance that the PC must travel to   *
 * get to the trainer (doesn't account for crossing buildings).  This is  *
 * not the distance from the NPC to the PC unless the NPC is a rival.     *
 *                                                                        *
 * Not a bug.                                                             *
 **************************************************************************/
static int compare_trainer_distance(const void *v1, const void *v2)
{
  const Character *const *c1 = (const Character *const *) v1;
  const Character *const *c2 = (const Character *const *) v2;

  return (world.rival_dist[(*c1)->pos[dim_y]][(*c1)->pos[dim_x]] -
          world.rival_dist[(*c2)->pos[dim_y]][(*c2)->pos[dim_x]]);
}

static Character *io_nearest_visible_trainer()
{
  Character **c, *n;
  uint32_t x, y, count;

  c = (Character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  n = c[0];

  free(c);

  return n;
}

void io_display()
{
  uint32_t y, x;
  Character *c;

  clear();
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.cur_map->cmap[y][x]) {
        mvaddch(y + 1, x, world.cur_map->cmap[y][x]->symbol);
      } else {
        switch (world.cur_map->map[y][x]) {
        case ter_boulder:
        case ter_mountain:
          attron(COLOR_PAIR(COLOR_MAGENTA));
          mvaddch(y + 1, x, '%');
          attroff(COLOR_PAIR(COLOR_MAGENTA));
          break;
        case ter_tree:
        case ter_forest:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '^');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_path:
        case ter_exit:
          attron(COLOR_PAIR(COLOR_YELLOW));
          mvaddch(y + 1, x, '#');
          attroff(COLOR_PAIR(COLOR_YELLOW));
          break;
        case ter_mart:
          attron(COLOR_PAIR(COLOR_BLUE));
          mvaddch(y + 1, x, 'M');
          attroff(COLOR_PAIR(COLOR_BLUE));
          break;
        case ter_center:
          attron(COLOR_PAIR(COLOR_RED));
          mvaddch(y + 1, x, 'C');
          attroff(COLOR_PAIR(COLOR_RED));
          break;
        case ter_grass:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, ':');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_clearing:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '.');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          attron(COLOR_PAIR(COLOR_CYAN));
          mvaddch(y + 1, x, '0');
          attroff(COLOR_PAIR(COLOR_CYAN)); 
       }
      }
    }
  }

  mvprintw(23, 1, "PC position is (%2d,%2d) on map %d%cx%d%c.",
           world.pc.pos[dim_x],
           world.pc.pos[dim_y],
           abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_x] - (WORLD_SIZE / 2) >= 0 ? 'E' : 'W',
           abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_y] - (WORLD_SIZE / 2) <= 0 ? 'N' : 'S');
  mvprintw(22, 1, "%d known %s.", world.cur_map->num_trainers,
           world.cur_map->num_trainers > 1 ? "trainers" : "trainer");
  mvprintw(22, 30, "Nearest visible trainer: ");
  if ((c = io_nearest_visible_trainer())) {
    attron(COLOR_PAIR(COLOR_RED));
    mvprintw(22, 55, "%c at %d %c by %d %c.",
             c->symbol,
             abs(c->pos[dim_y] - world.pc.pos[dim_y]),
             ((c->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              'N' : 'S'),
             abs(c->pos[dim_x] - world.pc.pos[dim_x]),
             ((c->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              'W' : 'E'));
    attroff(COLOR_PAIR(COLOR_RED));
  } else {
    attron(COLOR_PAIR(COLOR_BLUE));
    mvprintw(22, 55, "NONE.");
    attroff(COLOR_PAIR(COLOR_BLUE));
  }

  io_print_message_queue(0, 0);

  refresh();
}

uint32_t io_teleport_pc(pair_t dest)
{
  /* Just for fun. And debugging.  Mostly debugging. */

  do {
    dest[dim_x] = rand_range(1, MAP_X - 2);
    dest[dim_y] = rand_range(1, MAP_Y - 2);
  } while (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]                  ||
           move_cost[char_pc][world.cur_map->map[dest[dim_y]]
                                                [dest[dim_x]]] == INT_MAX ||
           world.rival_dist[dest[dim_y]][dest[dim_x]] < 0);

  return 0;
}

static void io_scroll_trainer_list(char (*s)[40], uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-40s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}

static void io_list_trainers_display(Npc **c,
                                     uint32_t count)
{
  uint32_t i;
  char (*s)[40]; /* pointer to array of 40 char */

  s = (char (*)[40]) malloc(count * sizeof (*s));

  mvprintw(3, 19, " %-40s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 40, "You know of %d trainers:", count);
  mvprintw(4, 19, " %-40s ", s[0]);
  mvprintw(5, 19, " %-40s ", "");

  for (i = 0; i < count; i++) {
    snprintf(s[i], 40, "%16s %c: %2d %s by %2d %s",
             char_type_name[c[i]->ctype],
             c[i]->symbol,
             abs(c[i]->pos[dim_y] - world.pc.pos[dim_y]),
             ((c[i]->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              "North" : "South"),
             abs(c[i]->pos[dim_x] - world.pc.pos[dim_x]),
             ((c[i]->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              "West" : "East"));
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      mvprintw(i + 6, 19, " %-40s ", s[i]);
    }
  }

  if (count <= 13) {
    mvprintw(count + 6, 19, " %-40s ", "");
    mvprintw(count + 7, 19, " %-40s ", "Hit escape to continue.");
    while (getch() != 27 /* escape */)
      ;
  } else {
    mvprintw(19, 19, " %-40s ", "");
    mvprintw(20, 19, " %-40s ",
             "Arrows to scroll, escape to continue.");
    io_scroll_trainer_list(s, count);
  }

  free(s);
}

static void io_list_trainers()
{
  Character **c;
  uint32_t x, y, count;

  c = (Character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  /* Display it */
  io_list_trainers_display((Npc **)(c), count);
  free(c);

  /* And redraw the map */
  io_display();
}

void io_pokemart()
{
  mvprintw(0, 0, "Welcome to the Pokemart.  Could I interest you in some Pokeballs?");
  refresh();
  getch();
}

void io_pokemon_center()
{
  mvprintw(0, 0, "Welcome to the Pokemon Center.  How can Nurse Joy assist you?");
  refresh();
  getch();
}

void clear_window()
{
	for(int i = 3; i <= 18; i++)
 	{
 		mvprintw(i, 7, " %-65s ", "");
		/* Borrow the first element of our array for this string: */
		mvprintw(i, 7, " %-65s ", "");
		mvprintw(i, 7, " %-65s ", "");
 	}
 	refresh();
}

void io_battle_choice(Npc *npc, Pokemon *p, int trainer_poke, int pc_poke)
{
	  clear_window();
		mvprintw(3, 7, "Your Pokemon:");
		mvprintw(4, 7, "%s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d",
                 world.pc.poke.at(pc_poke)->is_shiny() ? "*" : "", world.pc.poke.at(pc_poke)->get_species(),
                 world.pc.poke.at(pc_poke)->is_shiny() ? "*" : "", world.pc.poke.at(pc_poke)->get_hp(), world.pc.poke.at(pc_poke)->get_atk(), world.pc.poke.at(pc_poke)->get_def(), world.pc.poke.at(pc_poke)->get_spatk(), world.pc.poke.at(pc_poke)->get_spdef(), world.pc.poke.at(pc_poke)->get_speed());
    
    if(!p)
    {    
		  mvprintw(6, 7, "Trainer Pokemon:");
		  mvprintw(7, 7, "%s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d",
		               npc->poke.at(trainer_poke)->is_shiny() ? "*" : "", npc->poke.at(trainer_poke)->get_species(),
		               npc->poke.at(trainer_poke)->is_shiny() ? "*" : "", npc->poke.at(trainer_poke)->get_hp(), npc->poke.at(trainer_poke)->get_atk(), npc->poke.at(trainer_poke)->get_def(), npc->poke.at(trainer_poke)->get_spatk(), npc->poke.at(trainer_poke)->get_spdef(), npc->poke.at(trainer_poke)->get_speed());
		   mvprintw(9, 7, "Choose option:");
			mvprintw(10, 7, "1. Fight");
			mvprintw(11, 7, "2. Bag");
			mvprintw(12, 7, "3. Pokemon");
			refresh();
    }
    
    else
    {
    	mvprintw(6, 7, "Encountered Pokemon");
  		mvprintw(7, 7, "%s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d %s",
                   p->is_shiny() ? "*" : "", p->get_species(),
                   p->is_shiny() ? "*" : "", p->get_hp(), p->get_atk(),
                   p->get_def(), p->get_spatk(), p->get_spdef(),
                   p->get_speed(), p->get_gender_string());
     	mvprintw(9, 7, "Choose option:");
			mvprintw(10, 7, "1. Fight");
			mvprintw(11, 7, "2. Bag");
			mvprintw(12, 7, "3. Run");
			mvprintw(13, 7, "4. Pokemon");
    	refresh();
    }
}

void io_encounter_pokemon()
{
  Pokemon *p;
  
  int md = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
            abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)));
  int minl, maxl;
  
  if (md <= 200) {
    minl = 1;
    maxl = md / 2;
  } else {
    minl = (md - 200) / 2;
    maxl = 100;
  }
  if (minl < 1) {
    minl = 1;
  }
  if (minl > 100) {
    minl = 100;
  }
  if (maxl < 1) {
    maxl = 1;
  }
  if (maxl > 100) {
    maxl = 100;
  }

  p = new Pokemon(rand() % (maxl - minl + 1) + minl);

	int pc_poke = 0;
	int attempt = 0;
	int pc_turn = 0;
	int p_turn = 0;
	while(p)
	{
		io_battle_choice(NULL, p, 0, pc_poke);
		char input = getch();
		if(input == '1')
      {
      	if(world.pc.poke.at(pc_poke)->is_knocked())
      	{
      		clear_window();
      		mvprintw(11, 20, "Pokemon is knocked, cannot fight with this Pokemon");
      		mvprintw(12, 28, "Heal or choose another Pokemon");
      		refresh();
      		getch();
      		continue;
      	}
      	
      	if(p->is_knocked())
      	{
      		delete p;
      		break;
      	}
      	int rand_p_move;
      	if(p->get_num_moves() > 1)
      		rand_p_move = rand() % 2;
      	
      	clear_window();
      	Pokemon *pc_pokemon = world.pc.poke.at(pc_poke);
      	int moves = pc_pokemon->get_num_moves();
      	int move_choice;
      	int move_hit = rand() % 100;
      	
      	if(pc_turn)
      	{
      		if(moves > 1)
		    	{
		    		mvprintw(9, 28, "Choose move:");
		    		for(int j = 0; j < moves; j++)
		    			mvprintw(10 + j, 28, "%d. %s", j + 1, pc_pokemon->get_move(j));
		    			
		    		refresh();
		    		move_choice = getch() - 49;
		    	}
		    	
      		if(move_hit < pc_pokemon->get_move_acc(move_choice))
		    	{
				  	int critical = 1;
				  	if(pc_pokemon->get_base_speed() / 2 > rand() % 255)
				  		critical = 1.5;
				  		
				  	double random = ((rand() % 16) + 85) / 100.0;
				  	
				  	int stab = 1;
				  	int type = 1;
				  	for(int i = 0; i < static_cast<int>(pc_pokemon->type.size()); i++)
				  		if(pc_pokemon->get_type(i) == pc_pokemon->get_move_type(move_choice))
				  			stab = 1.5;
				  			
				  	//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
				  	//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
				  	//		if(pc_pokemon.get_type(i) == p->get_type(j))
				  	//			type = 2;
				  	
				  	int damage = ((((((2 * pc_pokemon->get_level()) / 5) + 2) * pc_pokemon->get_move_power(move_choice) * (pc_pokemon->get_atk() / p->get_def())) / 50) + 2) * critical * random * stab * type;
				  	
				  	clear_window();
				  	int old_hp = p->get_hp();
				  	p->subtract_hp(damage);
				  	int new_hp = p->get_hp();
				  	mvprintw(10, 28, "You did %d Damage!", damage);
				  	mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
				  	refresh();
				  	getch();
				  }
				  
				  else
				  {
				  	clear_window();
				  	mvprintw(11, 28, "Your attack missed!");
				  	refresh();
				  	getch();
				  }
				  pc_turn = 0;
				  p_turn = 1;
      	}
      	
      	else if(p_turn)
      	{
      		if(move_hit < p->get_move_acc(rand_p_move))
		    	{
				  	int critical = 1;
				  	if(p->get_base_speed() / 2 > rand() % 255)
				  		critical = 1.5;
				  		
				  	double random = ((rand() % 16) + 85) / 100.0;
				  	
				  	int stab = 1;
				  	int type = 1;
				  	for(int i = 0; i < static_cast<int>(p->type.size()); i++)
				  		if(p->get_type(i) == p->get_move_type(rand_p_move))
				  			stab = 1.5;
				  			
				  	//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
				  	//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
				  	//		if(pc_pokemon.get_type(i) == p->get_type(j))
				  	//			type = 2;
				  	
				  	int damage = ((((((2 * p->get_level()) / 5) + 2) * p->get_move_power(rand_p_move) * (p->get_atk() / pc_pokemon->get_def())) / 50) + 2) * critical * random * stab * type;
				  	
				  	clear_window();
				  	int old_hp = pc_pokemon->get_hp();
				  	pc_pokemon->subtract_hp(damage);
				  	int new_hp = pc_pokemon->get_hp();
				  	mvprintw(10, 28, "You took %d Damage!", damage);
				  	mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
				  	refresh();
				  	getch();
				  }
				  
				  else
				  {
				  	clear_window();
				  	mvprintw(11, 28, "Thier attack missed!");
				  	refresh();
				  	getch();
				  }
				  pc_turn = 1;
				  p_turn = 0;
      	}
      	
      	else
      	{
      		if(p->get_move_priority(rand_p_move) < pc_pokemon->get_move_priority(move_choice))
      		{
      			if(moves > 1)
				  	{
				  		mvprintw(9, 28, "Choose move:");
				  		for(int j = 0; j < moves; j++)
				  			mvprintw(10 + j, 28, "%d. %s", j + 1, pc_pokemon->get_move(j));
				  			
				  		refresh();
				  		move_choice = getch() - 49;
				  	}
				  	
		    		if(move_hit < pc_pokemon->get_move_acc(move_choice))
				  	{
							int critical = 1;
							if(pc_pokemon->get_base_speed() / 2 > rand() % 255)
								critical = 1.5;
								
							double random = ((rand() % 16) + 85) / 100.0;
							
							int stab = 1;
							int type = 1;
							for(int i = 0; i < static_cast<int>(pc_pokemon->type.size()); i++)
								if(pc_pokemon->get_type(i) == pc_pokemon->get_move_type(move_choice))
									stab = 1.5;
									
							//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
							//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
							//		if(pc_pokemon.get_type(i) == p->get_type(j))
							//			type = 2;
							
							int damage = ((((((2 * pc_pokemon->get_level()) / 5) + 2) * pc_pokemon->get_move_power(move_choice) * (pc_pokemon->get_atk() / p->get_def())) / 50) + 2) * critical * random * stab * type;
							
							clear_window();
							int old_hp = p->get_hp();
							p->subtract_hp(damage);
							int new_hp = p->get_hp();
							mvprintw(10, 28, "You did %d Damage!", damage);
							mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
							refresh();
							getch();
						}
						
						else
						{
							clear_window();
							mvprintw(11, 28, "Your attack missed!");
							refresh();
							getch();
						}
						pc_turn = 0;
				  	p_turn = 1;
		    	}
		    	
		    	else
		    	{
		    		if(move_hit < p->get_move_acc(rand_p_move))
				  	{
							int critical = 1;
							if(p->get_base_speed() / 2 > rand() % 255)
								critical = 1.5;
								
							double random = ((rand() % 16) + 85) / 100.0;
							
							int stab = 1;
							int type = 1;
							for(int i = 0; i < static_cast<int>(p->type.size()); i++)
								if(p->get_type(i) == p->get_move_type(rand_p_move))
									stab = 1.5;
									
							//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
							//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
							//		if(pc_pokemon.get_type(i) == p->get_type(j))
							//			type = 2;
							
							int damage = ((((((2 * p->get_level()) / 5) + 2) * p->get_move_power(rand_p_move) * (p->get_atk() / pc_pokemon->get_def())) / 50) + 2) * critical * random * stab * type;
							
							clear_window();
							int old_hp = pc_pokemon->get_hp();
							pc_pokemon->subtract_hp(damage);
							int new_hp = pc_pokemon->get_hp();
							mvprintw(10, 28, "You took %d Damage!", damage);
							mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
							refresh();
							getch();
						}
						
						else
						{
							clear_window();
							mvprintw(11, 28, "Thier attack missed!");
							refresh();
							getch();
						}
						pc_turn = 1;
				  	p_turn = 0;
		    	}
      	}   
      }
      
      else if(input == '2')
      {
      	int poke_captured = 0;
      	int action_taken = 0;
      	while(!action_taken)
      	{
		    	clear_window();
		    	mvprintw(9, 30, "Select Item");
		    	mvprintw(10, 30, "1. Pokeballs: %d", world.pc.num_pokeballs);
		    	mvprintw(11, 30, "2. Potions: %d", world.pc.num_potions);
		    	mvprintw(12, 30, "3. Revives: %d", world.pc.num_revives);
		    	mvprintw(18, 7, "Press any other key to exit");
		    	refresh();
		    	int choice = getch() - 48;
		    	if(choice == 1)
		    	{
		  			if(world.pc.num_poke == 6)
		  			{
		  				clear_window();
		  				mvprintw(11, 26, "The Pokemon got away!");
		  				refresh();
		  				getch();
		  				action_taken = 1;
		  				delete p;
		  			}
		  			else
		  			{
		  				clear_window();
		  				mvprintw(11, 26, "You captured the Pokemon!");
		  				refresh();
		  				getch();
		  				action_taken = 1;
		  				world.pc.num_poke++;
							world.pc.poke.push_back(p);
							world.pc.num_pokeballs--;
							poke_captured = 1;
							action_taken = 1;
		  			}
		    	}
		    	
		    	else if(choice == 2)
		    	{
		    		if(world.pc.poke.at(pc_poke)->is_knocked())
		    		{
		    			clear_window();
		    			mvprintw(11, 28, "Pokemon needs to be revived");
		    			refresh();
		    			getch();
		    			continue;
		    		}
		    		if(world.pc.poke.at(pc_poke)->get_hp() + 20 > world.pc.poke.at(pc_poke)->get_max_hp())
		    		{
		    			if((world.pc.poke.at(pc_poke)->get_max_hp() - world.pc.poke.at(pc_poke)->get_hp()) == 0)
		    			{
		    				mvprintw(14, 30, "Pokemon at max hp");
		    				refresh();
		    				getch();
		    			}
		    			else
		    			{
				  			world.pc.poke.at(pc_poke)->set_hp(world.pc.poke.at(pc_poke)->get_max_hp());
				  			world.pc.num_potions--;
				  			action_taken = 1;
		    			}
		    		}
		    		
		    		else
		    		{
		    			world.pc.poke.at(pc_poke)->add_hp(20);
		    			world.pc.num_potions--;
		    			action_taken = 1;
		    		}
		    	}
		    	
		    	else if(choice == 3)
		    	{
		    		if(world.pc.poke.at(pc_poke)->is_knocked())
		    		{
				  		world.pc.poke.at(pc_poke)->set_hp((world.pc.poke.at(pc_poke)->get_max_hp()) / 2);
				  		world.pc.num_revives--;
				  		action_taken = 1;
				  	}
				  	else
				  	{
				  		mvprintw(14, 30, "Pokemon cannot be revived.");
				  		refresh();
				  		getch();
				  	}
		    	}
		    	else
		    		break;
		    }
		    if(poke_captured == 1)
		    		break;
      }
      
      else if(input == '3')
      {
      	attempt++;
      	int escape_odd = ((world.pc.poke.at(pc_poke)->get_speed() * 32) / ((p->get_speed() / 4) % 256)) + (30 * attempt);
      	if(rand() % 256 <= escape_odd)
      	{
      		clear_window();
      		mvprintw(11, 30, "You ran away!");
      		refresh();
      		getch();
      		delete p;
      		break;
      	}
      	
      	else
      	{
      		clear_window();
      		mvprintw(11, 30, "You did not run away!");
      		refresh();
      		getch();
      	}
      }
      
      else
      {
      	clear_window();
      	mvprintw(10 - world.pc.num_poke, 30, "Choose pokemon");
      	for(int j = 0; j < world.pc.num_poke; j++)
      	{
      		mvprintw(11 - world.pc.num_poke + j, 13, "%d. %s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d", j + 1,
                   world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_species(),
                   world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_hp(), world.pc.poke.at(j)->get_atk(),
                   world.pc.poke.at(j)->get_def(), world.pc.poke.at(j)->get_spatk(), world.pc.poke.at(j)->get_spdef(),
                   world.pc.poke.at(j)->get_speed());
      	}
      	
      	refresh();
      	int choice = getch();
      	pc_poke = choice - 49;
      }
	}
}

void io_world_bag()
{
	clear_window();
	mvprintw(9, 30, "Select Item");
	mvprintw(10, 30, "1. Pokeballs: %d", world.pc.num_pokeballs);
	mvprintw(11, 30, "2. Potions: %d", world.pc.num_potions);
	mvprintw(12, 30, "3. Revives: %d", world.pc.num_revives);
	mvprintw(18, 7, "Press any other key to exit");
	refresh();
	int input = getch() - 48;
	
	if(input == 1)
	{
		clear_window();
		mvprintw(11, 30, "Cannot use a Pokeball");
		refresh();
		getch();
	}
	
	else if(input == 2)
	{
		if(world.pc.num_potions != 0)
		{
			clear_window();
			mvprintw(10 - world.pc.num_poke, 30, "Choose pokemon to heal");
			for(int j = 0; j < world.pc.num_poke; j++)
			{
				mvprintw(11 - world.pc.num_poke + j, 13, "%d. %s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d", j + 1,
		             world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_species(),
		             world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_hp(), world.pc.poke.at(j)->get_atk(),
		             world.pc.poke.at(j)->get_def(), world.pc.poke.at(j)->get_spatk(), world.pc.poke.at(j)->get_spdef(),
		             world.pc.poke.at(j)->get_speed());
			}
			
			refresh();
			int choice = getch() - 49;
			
			if(world.pc.poke.at(choice)->get_hp() + 20 > world.pc.poke.at(choice)->get_max_hp())
			{
				if((world.pc.poke.at(choice)->get_max_hp() - world.pc.poke.at(choice)->get_hp()) == 0)
				{
					mvprintw(14, 30, "Pokemon at max hp");
					refresh();
					getch();
				}
				else
				{
					int old_hp = world.pc.poke.at(choice)->get_hp();
					world.pc.poke.at(choice)->set_hp(world.pc.poke.at(choice)->get_max_hp());
					world.pc.num_potions--;
					mvprintw(14, 30, "%s healed!", world.pc.poke.at(choice)->get_species());
					mvprintw(15, 30, "%d -> %d", old_hp, world.pc.poke.at(choice)->get_hp());
					refresh();
					getch();
				}
			}
			
			else
			{
				int old_hp = world.pc.poke.at(choice)->get_hp();
				world.pc.poke.at(choice)->add_hp(20);
				world.pc.num_potions--;
				mvprintw(14, 30, "%s healed!", world.pc.poke.at(choice)->get_species());
				mvprintw(15, 30, "%d -> %d", old_hp, world.pc.poke.at(choice)->get_hp());
				refresh();
				getch();
			}
		}
		
		else
		{
			clear_window();
			mvprintw(11, 28, "You're out of Potions!");
			refresh();
			getch();
		}
	}
	
	else if(input == 3)
	{
		if(world.pc.num_revives != 0)
		{
			clear_window();
			mvprintw(10 - world.pc.num_poke, 30, "Choose pokemon to revive");
			for(int j = 0; j < world.pc.num_poke; j++)
			{
				mvprintw(11 - world.pc.num_poke + j, 13, "%d. %s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d", j + 1,
		             world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_species(),
		             world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_hp(), world.pc.poke.at(j)->get_atk(),
		             world.pc.poke.at(j)->get_def(), world.pc.poke.at(j)->get_spatk(), world.pc.poke.at(j)->get_spdef(),
		             world.pc.poke.at(j)->get_speed());
			}
			
			refresh();
			int choice = getch() - 49;
			
			if(!world.pc.poke.at(choice)->is_knocked())
			{
				clear_window();
				mvprintw(11, 20, "Pokemon is not knocked, cannot use revive");
				refresh();
				getch();
			}
			
			else
			{
				int old_hp = world.pc.poke.at(choice)->get_hp();
				world.pc.poke.at(choice)->add_hp(world.pc.poke.at(choice)->get_max_hp() / 2);
				world.pc.num_potions--;
				mvprintw(14, 30, "%s revived!", world.pc.poke.at(choice)->get_species());
				mvprintw(15, 30, "%d -> %d", old_hp, world.pc.poke.at(choice)->get_hp());
				refresh();
				getch();
			}
		}
		
		else
		{
			clear_window();
			mvprintw(11, 28, "You're out of Revives!");
			refresh();
			getch();
		}
	}
	
	else
		return;
}

void io_battle(Character *aggressor, Character *defender)
{
  Npc *npc;
	int pc_turn = 1;
	int npc_turn = 0;
	
  if (!(npc = dynamic_cast<Npc *>(aggressor))) {
    npc = dynamic_cast<Npc *>(defender);
    npc_turn = 0;
    pc_turn = 1;
  }
  
  int trainer_poke = 0;
  int pc_poke = 0;
  while(npc->defeated == 0)
  	{
  		io_battle_choice(npc, NULL, trainer_poke, pc_poke);
      char input = getch();
      int move_hit = rand() % 100;
      if(input == '1')
      {
      	if(world.pc.poke.at(pc_poke)->is_knocked())
      	{
      		clear_window();
      		mvprintw(11, 20, "Pokemon is knocked, cannot fight with this Pokemon");
      		mvprintw(12, 28, "Heal or choose another Pokemon");
      		refresh();
      		getch();
      		continue;
      	}
      	
      	clear_window();
      	Pokemon *pc_pokemon = world.pc.poke.at(pc_poke);
      	Pokemon *npc_poke = npc->poke.at(trainer_poke);
      	int moves = pc_pokemon->get_num_moves();
      	int move_choice;
      	
      	int rand_npc_move;
      	if(npc_poke->get_num_moves() > 1)
      		rand_npc_move = rand() % 2;
      	
      	if(pc_turn)
      	{
      		if(moves > 1)
		    	{
		    		mvprintw(9, 28, "Choose move:");
		    		for(int j = 0; j < moves; j++)
		    			mvprintw(10 + j, 28, "%d. %s", j + 1, pc_pokemon->get_move(j));
		    			
		    		refresh();
		    		move_choice = getch() - 49;
		    	}
		    	
      		if(move_hit < pc_pokemon->get_move_acc(move_choice))
		    	{
				  	int critical = 1;
				  	if(pc_pokemon->get_base_speed() / 2 > rand() % 255)
				  		critical = 1.5;
				  		
				  	double random = ((rand() % 16) + 85) / 100.0;
				  	
				  	int stab = 1;
				  	int type = 1;
				  	for(int i = 0; i < static_cast<int>(pc_pokemon->type.size()); i++)
				  		if(pc_pokemon->get_type(i) == pc_pokemon->get_move_type(move_choice))
				  			stab = 1.5;
				  			
				  	//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
				  	//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
				  	//		if(pc_pokemon.get_type(i) == p->get_type(j))
				  	//			type = 2;
				  	
				  	int damage = ((((((2 * pc_pokemon->get_level()) / 5) + 2) * pc_pokemon->get_move_power(move_choice) * (pc_pokemon->get_atk() / npc_poke->get_def())) / 50) + 2) * critical * random * stab * type;
				  	
				  	clear_window();
				  	int old_hp = npc_poke->get_hp();
				  	npc_poke->subtract_hp(damage);
				  	int new_hp = npc_poke->get_hp();
				  	mvprintw(10, 28, "You did %d Damage!", damage);
				  	mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
				  	refresh();
				  	getch();
				  }
				  
				  else
				  {
				  	clear_window();
				  	mvprintw(11, 28, "Your attack missed!");
				  	refresh();
				  	getch();
				  }
				  pc_turn = 0;
				  npc_turn = 1;
      	}
      	else if(npc_turn)
      	{
      		if(move_hit < npc_poke->get_move_acc(rand_npc_move))
		    	{
				  	int critical = 1;
				  	if(npc_poke->get_base_speed() / 2 > rand() % 255)
				  		critical = 1.5;
				  		
				  	double random = ((rand() % 16) + 85) / 100.0;
				  	
				  	int stab = 1;
				  	int type = 1;
				  	for(int i = 0; i < static_cast<int>(npc_poke->type.size()); i++)
				  		if(npc_poke->get_type(i) == npc_poke->get_move_type(rand_npc_move))
				  			stab = 1.5;
				  			
				  	//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
				  	//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
				  	//		if(pc_pokemon.get_type(i) == p->get_type(j))
				  	//			type = 2;
				  	
				  	int damage = ((((((2 * npc_poke->get_level()) / 5) + 2) * npc_poke->get_move_power(rand_npc_move) * (npc_poke->get_atk() / pc_pokemon->get_def())) / 50) + 2) * critical * random * stab * type;
				  	
				  	clear_window();
				  	int old_hp = pc_pokemon->get_hp();
				  	pc_pokemon->subtract_hp(damage);
				  	int new_hp = pc_pokemon->get_hp();
				  	mvprintw(10, 28, "You took %d Damage!", damage);
				  	mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
				  	refresh();
				  	getch();
				  }
				  
				  else
				  {
				  	clear_window();
				  	mvprintw(11, 28, "Thier attack missed!");
				  	refresh();
				  	getch();
				  }
				  pc_turn = 1;
				  npc_turn = 0;
      	}
      	
      	else
      	{
      		if(npc_poke->get_move_priority(rand_npc_move) < pc_pokemon->get_move_priority(move_choice))
      		{
      			if(moves > 1)
				  	{
				  		mvprintw(9, 28, "Choose move:");
				  		for(int j = 0; j < moves; j++)
				  			mvprintw(10 + j, 28, "%d. %s", j + 1, pc_pokemon->get_move(j));
				  			
				  		refresh();
				  		move_choice = getch() - 49;
				  	}
				  	
		    		if(move_hit < pc_pokemon->get_move_acc(move_choice))
				  	{
							int critical = 1;
							if(pc_pokemon->get_base_speed() / 2 > rand() % 255)
								critical = 1.5;
								
							double random = ((rand() % 16) + 85) / 100.0;
							
							int stab = 1;
							int type = 1;
							for(int i = 0; i < static_cast<int>(pc_pokemon->type.size()); i++)
								if(pc_pokemon->get_type(i) == pc_pokemon->get_move_type(move_choice))
									stab = 1.5;
									
							//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
							//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
							//		if(pc_pokemon.get_type(i) == p->get_type(j))
							//			type = 2;
							
							int damage = ((((((2 * pc_pokemon->get_level()) / 5) + 2) * pc_pokemon->get_move_power(move_choice) * (pc_pokemon->get_atk() / npc_poke->get_def())) / 50) + 2) * critical * random * stab * type;
							
							clear_window();
							int old_hp = npc_poke->get_hp();
							npc_poke->subtract_hp(damage);
							int new_hp = npc_poke->get_hp();
							mvprintw(10, 28, "You did %d Damage!", damage);
							mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
							refresh();
							getch();
						}
						
						else
						{
							clear_window();
							mvprintw(11, 28, "Your attack missed!");
							refresh();
							getch();
						}
						pc_turn = 0;
				  	npc_turn = 1;
		    	}
		    	
		    	else
		    	{
		    		if(move_hit < npc_poke->get_move_acc(rand_npc_move))
				  	{
							int critical = 1;
							if(npc_poke->get_base_speed() / 2 > rand() % 255)
								critical = 1.5;
								
							double random = ((rand() % 16) + 85) / 100.0;
							
							int stab = 1;
							int type = 1;
							for(int i = 0; i < static_cast<int>(npc_poke->type.size()); i++)
								if(npc_poke->get_type(i) == npc_poke->get_move_type(rand_npc_move))
									stab = 1.5;
									
							//for(int i = 0; i <  static_cast<int>(pc_pokemon.type.size()); i++)
							//	for(int j = 0; j <  static_cast<int>(p->type.size()); j++)
							//		if(pc_pokemon.get_type(i) == p->get_type(j))
							//			type = 2;
							
							int damage = ((((((2 * npc_poke->get_level()) / 5) + 2) * npc_poke->get_move_power(rand_npc_move) * (npc_poke->get_atk() / pc_pokemon->get_def())) / 50) + 2) * critical * random * stab * type;
							
							clear_window();
							int old_hp = pc_pokemon->get_hp();
							pc_pokemon->subtract_hp(damage);
							int new_hp = pc_pokemon->get_hp();
							mvprintw(10, 28, "You took %d Damage!", damage);
							mvprintw(11, 28, "HP: %d -> %d", old_hp, new_hp);
							refresh();
							getch();
						}
						
						else
						{
							clear_window();
							mvprintw(11, 28, "Thier attack missed!");
							refresh();
							getch();
						}
						pc_turn = 1;
				  	npc_turn = 0;
		    	}
		    }
      }
      else if(input == '2')
      {
      	int action_taken = 0;
      	while(!action_taken)
      	{
      		clear_window();
      		mvprintw(9, 30, "Select Item");
      		mvprintw(10, 30, "1. Potions: %d", world.pc.num_potions);
      		mvprintw(11, 30, "2. Revives: %d", world.pc.num_revives);
      		mvprintw(18, 7, "Press any other key to exit");
      		refresh();
      		int choice = getch() - 48;
      	
					if(choice == 1)
					{
						if(world.pc.num_potions != 0)
						{
							if(world.pc.poke.at(pc_poke)->is_knocked())
				  		{
				  			clear_window();
				  			mvprintw(11, 28, "Pokemon needs to be revived");
				  			refresh();
				  			getch();
				  			continue;
				  		}
				  		
							if(world.pc.poke.at(pc_poke)->get_hp() + 20 > world.pc.poke.at(pc_poke)->get_max_hp())
								{
									if((world.pc.poke.at(pc_poke)->get_max_hp() - world.pc.poke.at(pc_poke)->get_hp()) == 0)
									{
										mvprintw(14, 30, "Pokemon at max hp");
										refresh();
										getch();
									}
									else
									{
										world.pc.poke.at(pc_poke)->set_hp(world.pc.poke.at(pc_poke)->get_max_hp());
										world.pc.num_potions--;
										action_taken = 1;
									}
								}
								
								else
								{
									world.pc.poke.at(pc_poke)->add_hp(20);
									world.pc.num_potions--;
									action_taken = 1;
								}
							}
							
							else
							{
								clear_window();
								mvprintw(11, 28, "You're out of Potions");
								refresh();
								getch();
							}
						}
						
						else if(choice == 2)
						{
							if(world.pc.num_revives != 0)
							{
								if(world.pc.poke.at(pc_poke)->is_knocked())
								{
									world.pc.poke.at(pc_poke)->set_hp((world.pc.poke.at(pc_poke)->get_max_hp()) / 2);
									world.pc.num_revives--;
									action_taken = 1;
								}
								else
								{
									mvprintw(14, 30, "Pokemon cannot be revived.");
									refresh();
									getch();
								}
							}
							
							else
							{
								clear_window();
								mvprintw(11, 28, "You're out of revives!");
								refresh();
								getch();
							}
						}
						else
							break;
					}
				}
     
      else if(input == '3')
      {
      	do{
		    	clear_window();
		    	mvprintw(10 - world.pc.num_poke, 30, "Choose pokemon");
		    	for(int j = 0; j < world.pc.num_poke; j++)
		    	{
		    		mvprintw(11 - world.pc.num_poke + j, 13, "%d. %s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d", j + 1,
		                 world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_species(),
		                 world.pc.poke.at(j)->is_shiny() ? "*" : "", world.pc.poke.at(j)->get_hp(), world.pc.poke.at(j)->get_atk(),
		                 world.pc.poke.at(j)->get_def(), world.pc.poke.at(j)->get_spatk(), world.pc.poke.at(j)->get_spdef(),
		                 world.pc.poke.at(j)->get_speed());
		    	}
		    	
		    	refresh();
		    	int choice = getch();
		    	pc_poke = choice - 49;
		    	
		    	if(world.pc.poke.at(pc_poke)->is_knocked())
		    	{
		    		clear_window();
		    		mvprintw(14, 30, "Pokemon is knocked out");
		    		refresh();
		    		getch();
		    	}
		    }while(world.pc.poke.at(pc_poke)->is_knocked());
      }
     
      for(; trainer_poke < npc->num_poke; trainer_poke++)
      {
      	if(npc->poke.at(trainer_poke)->is_knocked())
      		continue;
      		
      	else
      		break;
      }
      
      if(trainer_poke == npc->num_poke)
      {
     		npc->defeated = 1;
  			if (npc->ctype == char_hiker || npc->ctype == char_rival) 
    			npc->mtype = move_wander;
    	}
  	}
  }

uint32_t move_pc_dir(uint32_t input, pair_t dest)
{
  dest[dim_y] = world.pc.pos[dim_y];
  dest[dim_x] = world.pc.pos[dim_x];

  switch (input) {
  case 1:
  case 2:
  case 3:
    dest[dim_y]++;
    break;
  case 4:
  case 5:
  case 6:
    break;
  case 7:
  case 8:
  case 9:
    dest[dim_y]--;
    break;
  }
  switch (input) {
  case 1:
  case 4:
  case 7:
    dest[dim_x]--;
    break;
  case 2:
  case 5:
  case 8:
    break;
  case 3:
  case 6:
  case 9:
    dest[dim_x]++;
    break;
  case '>':
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_mart) {
      io_pokemart();
    }
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_center) {
      io_pokemon_center();
    }
    break;
  }

  if ((world.cur_map->map[dest[dim_y]][dest[dim_x]] == ter_exit) &&
      (input == 1 || input == 3 || input == 7 || input == 9)) {
    // Exiting diagonally leads to complicated entry into the new map
    // in order to avoid INT_MAX move costs in the destination.
    // Most easily solved by disallowing such entries here.
    return 1;
  }

  if (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) {
    if (dynamic_cast<Npc *>(world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) &&
        ((Npc *) world.cur_map->cmap[dest[dim_y]][dest[dim_x]])->defeated) {
      // Some kind of greeting here would be nice
      return 1;
    } else if (dynamic_cast<Npc *>
               (world.cur_map->cmap[dest[dim_y]][dest[dim_x]])) {
      io_battle(&world.pc, world.cur_map->cmap[dest[dim_y]][dest[dim_x]]);
      // Not actually moving, so set dest back to PC position
      dest[dim_x] = world.pc.pos[dim_x];
      dest[dim_y] = world.pc.pos[dim_y];
    }
  }
  
  if (move_cost[char_pc][world.cur_map->map[dest[dim_y]][dest[dim_x]]] ==
      INT_MAX) {
    return 1;
  }

  return 0;
}

void io_teleport_world(pair_t dest)
{
  int x, y;
  
  world.cur_map->cmap[world.pc.pos[dim_y]][world.pc.pos[dim_x]] = NULL;

  mvprintw(0, 0, "Enter x [-200, 200]: ");
  refresh();
  echo();
  curs_set(1);
  mvscanw(0, 21, (char *) "%d", &x);
  mvprintw(0, 0, "Enter y [-200, 200]:          ");
  refresh();
  mvscanw(0, 21, (char *) "%d", &y);
  refresh();
  noecho();
  curs_set(0);

  if (x < -200) {
    x = -200;
  }
  if (x > 200) {
    x = 200;
  }
  if (y < -200) {
    y = -200;
  }
  if (y > 200) {
    y = 200;
  }
  
  x += 200;
  y += 200;

  world.cur_idx[dim_x] = x;
  world.cur_idx[dim_y] = y;

  new_map(1);
  io_teleport_pc(dest);
}

void io_pick_pokemon()
{
	Pokemon *p1 = new Pokemon(1);
  Pokemon *p2 = new Pokemon(1);
  Pokemon *p3 = new Pokemon(1);
	std::cout << "Pick a starting Pokemon\n1. " << p1->get_species() << "\n2. " <<  
  		p2->get_species() << "\n3. " << p3->get_species() << std::endl;
  		
  mvprintw(0,0, "Pick a starting Pokemon\n1. %s%s%s HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d %s\n2. %s%s%s HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d %s\n3. %s%s%s HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d %s\n",
                   p1->is_shiny() ? "*" : "", p1->get_species(),
                   p1->is_shiny() ? "*" : "", p1->get_hp(), p1->get_atk(),
                   p1->get_def(), p1->get_spatk(), p1->get_spdef(),
                   p1->get_speed(), p1->get_gender_string(),
                   p2->is_shiny() ? "*" : "", p2->get_species(),
                   p2->is_shiny() ? "*" : "", p2->get_hp(), p2->get_atk(),
                   p2->get_def(), p2->get_spatk(), p2->get_spdef(),
                   p2->get_speed(), p2->get_gender_string(),
                   p3->is_shiny() ? "*" : "", p3->get_species(),
                   p3->is_shiny() ? "*" : "", p3->get_hp(), p3->get_atk(),
                   p3->get_def(), p3->get_spatk(), p3->get_spdef(),
                   p3->get_speed(), p3->get_gender_string());
  char choice = getch();
  
  if(choice == '1')
  {
  	world.pc.poke.push_back(p1);
  	//world.pc.poke = p1;
  	delete p2;
  	delete p3;
  }
  else if(choice == '2')
  {
  	world.pc.poke.push_back(p2);
  	//world.pc.poke = p2;
  	delete p1;
  	delete p3;
  }
  else
  {
  		world.pc.poke.push_back(p3);
  	//world.pc.poke = p3;
  	delete p2;
  	delete p1;
  }
  world.pc.num_poke++;
}

void io_handle_input(pair_t dest)
{
  uint32_t turn_not_consumed;
  int key;

  do {
    switch (key = getch()) {
    case '7':
    case 'y':
    case KEY_HOME:
      turn_not_consumed = move_pc_dir(7, dest);
      break;
    case '8':
    case 'k':
    case KEY_UP:
      turn_not_consumed = move_pc_dir(8, dest);
      break;
    case '9':
    case 'u':
    case KEY_PPAGE:
      turn_not_consumed = move_pc_dir(9, dest);
      break;
    case '6':
    case 'l':
    case KEY_RIGHT:
      turn_not_consumed = move_pc_dir(6, dest);
      break;
    case '3':
    case 'n':
    case KEY_NPAGE:
      turn_not_consumed = move_pc_dir(3, dest);
      break;
    case '2':
    case 'j':
    case KEY_DOWN:
      turn_not_consumed = move_pc_dir(2, dest);
      break;
    case '1':
    case 'b':
    case KEY_END:
      turn_not_consumed = move_pc_dir(1, dest);
      break;
    case '4':
    case 'h':
    case KEY_LEFT:
      turn_not_consumed = move_pc_dir(4, dest);
      break;
    case '5':
    case ' ':
    case '.':
    case KEY_B2:
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    case '>':
      turn_not_consumed = move_pc_dir('>', dest);
      break;
    case 'Q':
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      world.quit = 1;
      turn_not_consumed = 0;
      break;
      break;
    case 't':
      /* Teleport the PC to a random place in the map.              */
      io_teleport_pc(dest);
      turn_not_consumed = 0;
      break;
    case 'T':
      /* Teleport the PC to any map in the world.                   */
      io_teleport_world(dest);
      turn_not_consumed = 0;
      break;
    case 'm':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'B':
    	io_world_bag();
    	turn_not_consumed = 1;
    	break;
    case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matters, but using this command will   *
       * waste a turn.  Set turn_not_consumed to 1 and you should be *
       * able to figure out why I did it that way.                   */
      io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      io_queue_message("Oh!  And use 'Q' to quit!");

      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      turn_not_consumed = 1;
    }
    refresh();
  } while (turn_not_consumed);
}
