# Poll IDFM estimated-timetable once a minute; keep one compact gzip CSV per poll
import os, sys, json, time, gzip, csv, urllib.request
n = int(sys.argv[1])
for i in range(n):
    t = time.time()
    try:
        req = urllib.request.Request('https://prim.iledefrance-mobilites.fr/marketplace/estimated-timetable?LineRef=ALL',
                                     headers={'apiKey': os.environ['IDFM_API_KEY'], 'Accept': 'application/json'})
        j = json.load(urllib.request.urlopen(req, timeout=50))
        d = j['Siri']['ServiceDelivery']
        v = lambda o: (o or {}).get('value', '') if isinstance(o, dict) else (o[0].get('value', '') if o else '')
        rows = 0
        with gzip.open(f'api/{int(t)}.csv.gz', 'wt', newline='') as f:
            w = csv.writer(f)
            w.writerow(['#', d.get('ResponseTimestamp'), t])
            for fr in d['EstimatedTimetableDelivery'][0]['EstimatedJourneyVersionFrame']:
                for vj in fr['EstimatedVehicleJourney']:
                    head = [v(vj.get('LineRef')), v(vj.get('PublishedLineName')), v(vj.get('DirectionName')), v(vj.get('DirectionRef')),
                            v(vj.get('DestinationRef')), v(vj.get('DestinationName')), v(vj.get('OperatorRef')),
                            ','.join(vj.get('VehicleMode') or []), v(vj.get('DatedVehicleJourneyRef')), vj.get('RecordedAtTime', '')]
                    for c in vj.get('EstimatedCalls', {}).get('EstimatedCall', []):
                        w.writerow(head + [v(c.get('StopPointRef')), c.get('ExpectedArrivalTime', ''), c.get('ExpectedDepartureTime', ''),
                                           c.get('AimedArrivalTime', ''), c.get('ArrivalStatus', '') or c.get('DepartureStatus', '')])
                        rows += 1
        print(time.strftime('%H:%M:%S'), 'ok', rows, flush=True)
    except Exception as e:
        print(time.strftime('%H:%M:%S'), 'ERR', repr(e)[:200], flush=True)
    time.sleep(max(0, 60 - (time.time() - t)))
