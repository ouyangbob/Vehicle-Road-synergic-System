#ifndef COMM_ROAD_BLOCK_H
#define COMM_ROAD_BLOCK_H

#ifdef COMM_ROAD_BLOCK_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* COMM_ROAD_BLOCK_C */

PUBLIC void RoadBlockProcess(void);

#undef PUBLIC

#endif /* COMM_ROAD_BLOCK_H */

/* end of file */
