/**
 * EvalScheme and Evaluator
 *
 * A board evaluation scheme using a combination of 3 evaluations:
 *  - Token difference
 *  - Place
 *  - Movability / Connectivity
 *
 * Coefficients used for these evaluations are variable.
 *
 * The constructor gets a name, and tries to read the coefficients
 * from a configuration file, if nothing found, use default values
 */


#ifndef EVAL_H
#define EVAL_H

#include "board.h"

class EvalScheme
{
 public:
  EvalScheme(char*);
  ~EvalScheme() {}

  void setDefaults();
  void read(char* file);
  void save(char* file);

  void setRingValue(int ring, int value);
  void setRingDiff(int ring, int value);
  void setStoneValue(int stoneDiff, int value);
  void setMoveValue(int type, int value);
  void setInARowValue(int stones, int value);

  int ringValue(int r) { return (r>=0 && r<5) ? _ringValue[r] : 0; }
  int ringDiff(int r) { return (r>0 && r<5) ? _ringDiff[r] : 0; }
  int stoneValue(int s) { return (s>0 && s<6) ? _stoneValue[s] : 0; }
  int moveValue(int t)
      { return (t>=0 && t<Move::typeCount) ? _moveValue[t] : 0;}
  int inARowValue(int s)
      { return (s>=0 && s<MoveCounter::inARowCount) ? _inARowValue[s]:0; }

 private:
  int _ringValue[5], _ringDiff[5];
  int _stoneValue[6], _moveValue[Move::none];
  int _inARowValue[MoveCounter::inARowCount];
};


class Evaluator
{
 public:

    /* different states of one field */
    enum {
	out = 10, free = 0,
	color1, color2, color1bright, color2bright
    };
    enum { AllFields = 121, /* visible + ring of unvisible around */
	   RealFields = 61, /* number of visible fields */
	   MvsStored = 100 };

    Evaluator();

    /* Evaluation Scheme to use */
    void setEvalScheme( EvalScheme* scheme = 0);
    EvalScheme* evalScheme() { return _evalScheme; }
    void setFieldValues();

    int minValue() { return -15000; }
    int maxValue() { return  15000; }

    /* Calculate a value for actual position
     * (greater if better for color1) */
    int calcEvaluation(Board*);

    /* Evalution is based on values which can be changed
     * a little (so computer's moves aren't always the same) */
    void changeEvaluation();

    void setBoard(Board*);

 private:
    Board* _board;
    EvalScheme* _evalScheme;
    int* field;

    /* ratings; semi constant - are rotated by changeRating() */
    static int fieldValue[Board::RealFields];
};

#endif
