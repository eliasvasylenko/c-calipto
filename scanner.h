struct scanner_api
{
  /**
   * return the input position at the head of the scan
   */
  long (*input_position)(void);

  /**
   * @return the position from which text is buffered, up to the input position
   */
  long (*buffer_position)(void);

  /**
   * @return the input position relative to the current retained position.
   */
  long (*buffer_size)(void);

  /**
   * Advance the input position while the character at that position matches the
   * given predicate.
   * 
   * @param condition
   * @return the count by which the input position was advanced
   */
  long (*advance_input_while)(bool (*condition)(character));

  /**
   * Advance the input position if the character at that position matches the
   * given predicate.
   * 
   * @param condition
   * @return true if the input position was advanced
   */
  bool (*advance_input_if)(bool (*condition)(character));

  /**
   * Take everything in the interval from the mark position to the given
   * position and reset the mark position to the given position.
   * 
   * @return the amount of buffer filled
   */
  int (*take_buffer_to)(character* string, int length, long position);

  /**
   * Take everything in the interval from the mark position to the input
   * position and reset the mark position to the input position.
   * 
   * @return the amount of buffer filled
   */
  int (*take_buffer)(character* string, int length);

  void (*discard_buffer_to)(long position);

  /**
   * Set the mark position to the current input position, discarding everything
   * prior.
   */
  void (*discard_buffer)(void);
}
