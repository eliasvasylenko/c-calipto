struct reader_api {
  long (*cursor_position)(int inputDepth);

  int (*cursor_depth)(void);

  void* (*read)(void);

  void* (*read_symbol)(void);

  bool (*read_step_in)(void);

  int (*read_step_out)(void);
};
