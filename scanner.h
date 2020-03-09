struct scanner_api {
  long (*input_position)(void);

  long (*buffer_position)(void);

  long (*buffer_size)(void);

  long (*advance_input_while)(bool (*condition)(character));

  bool (*advance_input_if)(bool (*condition)(character));

  int (*take_buffer_to)(character* string, int length, long position);

  void (*discard_buffer_to)(long position);

  void (*discard_buffer)(void);
};
