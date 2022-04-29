#ifndef POKEMON_H
# define POKEMON_H

# include <iostream>
# include <vector>

enum pokemon_stat {
  stat_hp,
  stat_atk,
  stat_def,
  stat_spatk,
  stat_spdef,
  stat_speed
};

enum pokemon_gender {
  gender_female,
  gender_male
};

class Pokemon {
 private:
  int level;
  int pokemon_index;
  int move_index[4];
  int pokemon_species_index;
  int IV[6];
  int effective_stat[6];
  bool shiny;
  pokemon_gender gender;
  int max_hp;
 public:
  Pokemon(int level);
  const char *get_species() const;
  int get_move_power(int i) const;
  int get_level() const;
  int get_hp() const;
  int get_atk() const;
  int get_def() const;
  int get_spatk() const;
  int get_spdef() const;
  int get_speed() const;
  int get_base_speed() const;
  void set_hp(int new_hp);
  void add_hp(int health);
  void subtract_hp(int damage);
  const char *get_gender_string() const;
  bool is_shiny() const;
  const char *get_move(int i) const;
  int get_num_moves() const;
  std::ostream &print(std::ostream &o) const;
  bool is_knocked() const;
  int get_max_hp() const;
  int get_type(int i) const;
  int get_move_type(int i) const;
  int get_move_acc(int i) const;
  int get_move_priority(int i) const;
 	std::vector<int> type;
};

std::ostream &operator<<(std::ostream &o, const Pokemon &p);

#endif
