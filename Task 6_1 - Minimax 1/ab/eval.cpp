/**
 * EvalScheme and Evaluator
 */

#include "eval.h"


// Default Values
static int defaultRingValue[] = { 45, 35, 25, 10, 0 };
static int defaultRingDiff[]  = {  0, 10, 10,  8, 5 };
static int defaultStoneValue[]= { 0,-800,-1800,-3000,-4400,-6000 };
static int defaultMoveValue[Move::typeCount] = { 40,30,30, 15,14,13, 
						 5,5,5, 2,2,2, 1 };
static int defaultInARowValue[MoveCounter::inARowCount]= { 2, 5, 4, 3 };


/**
 * Constructor: Set Default values
 */
EvalScheme::EvalScheme(char* file)
{
  setDefaults();
  read(file);
}


void EvalScheme::setDefaults()
{
  for(int i=0;i<6;i++)
    _stoneValue[i] = defaultStoneValue[i];

  for(int i=0;i<Move::typeCount;i++)
    _moveValue[i] = defaultMoveValue[i];

  for(int i=0;i<MoveCounter::inARowCount;i++)
    _inARowValue[i] = defaultInARowValue[i];
  
  for(int i=0;i<5;i++)
    _ringValue[i] = defaultRingValue[i];

  for(int i=0;i<5;i++)
    _ringDiff[i] = defaultRingDiff[i];
}
  

void EvalScheme::read(char* file)
{
    if (file == 0 || *file == 0) return;

    // TODO
}


void EvalScheme::save(char* file)
{
    // TODO
}

void EvalScheme::setRingValue(int ring, int value)
{
  if (ring >=0 && ring <5)
      _ringValue[ring] = value;
}

void EvalScheme::setRingDiff(int ring, int value)
{
  if (ring >=1 && ring <5)
    _ringDiff[ring] = value;
}

void EvalScheme::setStoneValue(int stoneDiff, int value)
{
  if (stoneDiff>0 && stoneDiff<6)
    _stoneValue[stoneDiff] = value;
}

void EvalScheme::setMoveValue(int type, int value)
{
  if (type>=0 && type<Move::typeCount)
    _moveValue[type] = value;
}

void EvalScheme::setInARowValue(int stones, int value)
{
  if (stones>=0 && stones<MoveCounter::inARowCount)
    _inARowValue[stones] = value;
}



/// Evaluator

int Evaluator::fieldValue[61];

Evaluator::Evaluator()
{
    _evalScheme = 0;
}

void Evaluator::setEvalScheme(EvalScheme* scheme)
{
  if (!scheme)
    scheme = new EvalScheme(0);

  _evalScheme = scheme;
  setFieldValues();
}

void Evaluator::setFieldValues()
{
  if (!_evalScheme) return;

  int i, j = 0, k = 59;
  int ringValue[5], ringDiff[5];

  for(i=0;i<5;i++) {
    ringDiff[i]  = _evalScheme->ringDiff(i);
    ringValue[i] = _evalScheme->ringValue(i);
    if (ringDiff[i]<1) ringDiff[i]=1;
  }

  fieldValue[0] = ringValue[0];
  for(i=1;i<7;i++)
    fieldValue[i] = ringValue[1] + ((j+=k) % ringDiff[1]);
  for(i=7;i<19;i++)
    fieldValue[i] = ringValue[2] + ((j+=k) % ringDiff[2]);
  for(i=19;i<37;i++)
    fieldValue[i] = ringValue[3] + ((j+=k) % ringDiff[3]);
  for(i=37;i<61;i++)
    fieldValue[i] = ringValue[4] + ((j+=k) % ringDiff[4]);
}

void Evaluator::setBoard(Board* b)
{
    _board = b;
    field = b->fieldArray();
}



/* Calculate a evaluation for actual position
 *
 * A higher value means a better position for opponent
 * NB: This means a higher value for better position of
 *     'color before last move'
 */
int Evaluator::calcEvaluation(Board* b)
{
    setBoard(b);
    int color = b->actColor();

    if (!_evalScheme) {
	// Use default values if not set
	setEvalScheme();
    }
    
  MoveCounter cColor, cOpponent;

  int f,i,j;

  /* different evaluation types */
  int fieldValueSum=0, stoneValueSum=0;
  int moveValueSum=0, inARowValueSum=0;
  int valueSum;

  /* First check simple winner condition */
  int color1Count = _board->getColor1Count();
  int color2Count = _board->getColor2Count();
  if (color1Count <9)
    valueSum = (color==color1) ? 16000 : -16000;
  else if (color2Count <9)
    valueSum = (color==color2) ? 16000 : -16000;
  else {

    /* Calculate fieldValueSum and count move types and connectivity */
    for(i=0;i<RealFields;i++) {
      j=field[f=Board::order[i]];
      if (j == free) continue;
      if (j == color) {
	b->countFrom( f, j, cColor );
	fieldValueSum -= fieldValue[i];
      }
      else {
	b->countFrom( f, j, cOpponent );
	fieldValueSum += fieldValue[i];
      }
    }

    /* If color can't do any moves, opponent wins... */
    if (cColor.moveSum() == 0)
      valueSum = 16000;
    else {

      for(int t=0;t < Move::typeCount;t++)
	moveValueSum += _evalScheme->moveValue(t) *
	  (cOpponent.moveCount(t) - cColor.moveCount(t));

      for(int i=0;i < MoveCounter::inARowCount;i++)
	inARowValueSum += _evalScheme->inARowValue(i) *
	  (cOpponent.rowCount(i) - cColor.rowCount(i));

      if (color == color2)
	stoneValueSum = _evalScheme->stoneValue(14 - color1Count) -
	  _evalScheme->stoneValue(14 - color2Count);
      else
	stoneValueSum = _evalScheme->stoneValue(14 - color2Count) -
	  _evalScheme->stoneValue(14 - color1Count);

      valueSum = fieldValueSum + moveValueSum +
	inARowValueSum + stoneValueSum;
    }
  }

#ifdef MYTRACE
  if (spyLevel>2) {
    indent(spyDepth);
    printf("Eval %d (field %d, move %d, inARow %d, stone %d)\n",
	   valueSum, fieldValueSum, moveValueSum,
	   inARowValueSum, stoneValueSum );
  }
#endif

  return valueSum;
}

void Evaluator::changeEvaluation()
{
  int i,tmp;

  /* innermost ring */
  tmp=fieldValue[1];
  for(i=1;i<6;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[6] = tmp;

  tmp=fieldValue[7];
  for(i=7;i<18;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[18] = tmp;

  tmp=fieldValue[19];
  for(i=19;i<36;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[36] = tmp;

  /* the outermost ring */
  tmp=fieldValue[37];
  for(i=37;i<60;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[60] = tmp;
}
