from scipy.optimize import fsolve, root, least_squares
import math

txPower = -59;

def estimateDistance(rssi: int):
  """
  estimate distance in meters.
  txPower is an [experimentally defined] transmission power in dBm.
  actual signal power on the beacon side is at +3 dBm.
  """
  ratio = rssi * 1.0 / txPower;
  if ratio < 1.0:
    return math.pow(ratio, 10);
  else:
    return 0.89976 * pow(ratio, 7.7095) + 0.111;

def estimateCoordinates(bs, ds):
  """
  estimates the coordinates of the reciever based on given beacon locations and measured distances.
  this is derived from the distance formula but scipy makes my life incredibly easy
  
  bs is an array of dicts representing beacons with two properties; x and y (as ints).\n
  ds is an array of signed int representing rssi distance values.

  initially this was done with two eqns that were solved with scipy's root method.
  this has changed to a linear least squares solution, because the equations derived from distance equation are
  not continuous, so that couldn't work.  
  """
  if len(bs) != len(ds):
    raise Exception("Lengths of beacons and distances between them are not equal.")
  
  def eqns(p):
    x, y = p
    return [
      # distance formula derived and executed over zipped bs and ds
      math.sqrt(math.pow(x - b["x"], 2) + math.pow(y - b["y"], 2)) - d
      for b, d in zip(bs, ds)
    ]

  # initial guess. use the average of beacon coords.
  x0 = sum(b['x'] for b in bs) / len(bs)
  y0 = sum(b['y'] for b in bs) / len(bs) 

  result = least_squares(eqns, [x0, y0])
  return tuple(result.x)
