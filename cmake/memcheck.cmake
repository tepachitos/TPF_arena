macro(add_memcheck_target DESIRED_TARGET)
  add_custom_target(${DESIRED_TARGET}_memcheck
    COMMAND valgrind
      --tool=memcheck
      --leak-check=full
      --show-leak-kinds=all
      --track-origins=yes
      --error-exitcode=1
      $<TARGET_FILE:${DESIRED_TARGET}>
      DEPENDS ${DESIRED_TARGET}
  )
endmacro()
