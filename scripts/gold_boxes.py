from __future__ import print_function
import json
import numpy as np
import struct
import sys
from pprint import pprint
import metadata_pb2
import evaluators.types_pb2
import scanner

def load_bboxes(dataset_name, job_name, column_name):
    def buf_to_bboxes(buf):
        (num_bboxes,) = struct.unpack("=Q", buf[:8])
        buf = buf[8:]
        (bbox_size,) = struct.unpack("=i", buf[:4])
        buf = buf[4:]
        bboxes = []
        for i in range(num_bboxes):
            box = evaluators.types_pb2.BoundingBox()
            box.ParseFromString(buf[:bbox_size])
            buf = buf[bbox_size:]
            bbox = [box.x1, box.y1, box.x2, box.y2, box.score,
                    box.track_id, box.track_score]
            bboxes.append(bbox)
        return bboxes
    return scanner.load_output_buffers(dataset_name, job_name, column_name, buf_to_bboxes)


def save_bboxes(dataset_name, job_name, ident, column_name, bboxes):
    def bboxes_to_buf(boxes):
        data = struct.pack("=Q", len(boxes))

        box = evaluators.types_pb2.BoundingBox()
        box.x1 = 0
        box.y1 = 0
        box.x2 = 0
        box.y2 = 0
        box.score = 0
        bbox_size = box.ByteSize()
        data += struct.pack("=i", bbox_size)

        for bbox in boxes:
            box = evaluators.types_pb2.BoundingBox()
            box.x1 = bbox[0]
            box.y1 = bbox[1]
            box.x2 = bbox[2]
            box.y2 = bbox[3]
            box.score = bbox[4]
            data += box.SerializeToString()
        return data

    return scanner.write_output_buffers(dataset_name, job_name, ident, column_name,
                                bboxes_to_buf, bboxes)


def iou(bl, br):
    blx1 = bl[0]
    bly1 = bl[1]
    blx2 = bl[2]
    bly2 = bl[3]

    brx1 = br[0]
    bry1 = br[1]
    brx2 = br[2]
    bry2 = br[3]

    x1 = max(blx1, brx1)
    y1 = max(bly1, bry1)
    x2 = min(blx2, brx2)
    y2 = min(bly2, bry2)

    if (x1 >= x2 or y1 >= y2):
        return 0

    intersection = (y2 - y1) * (x2 - x1)
    _union = ((blx2 - blx1) * (bly2 - bly1) +
              (brx2 - brx1) * (bry2 - bry1) -
              intersection)
    if _union == 0:
        return 0
    return intersection / _union


def nms(boxes, overlapThresh):
	# if there are no boxes, return an empty list
	if len(boxes) == 0:
		return []
        elif len(boxes) == 1:
            return boxes
 
        npboxes = np.array(boxes[0])
        for box in boxes[1:]:
            npboxes = np.vstack((npboxes, box))
        boxes = npboxes
	# if the bounding boxes integers, convert them to floats --
	# this is important since we'll be doing a bunch of divisions
 
	# initialize the list of picked indexes	
	pick = []
 
	# grab the coordinates of the bounding boxes
	x1 = boxes[:,0]
	y1 = boxes[:,1]
	x2 = boxes[:,2]
	y2 = boxes[:,3]
 
	# compute the area of the bounding boxes and sort the bounding
	# boxes by the bottom-right y-coordinate of the bounding box
	area = (x2 - x1 + 1) * (y2 - y1 + 1)
	idxs = np.argsort(y2)
 
	# keep looping while some indexes still remain in the indexes
	# list
	while len(idxs) > 0:
		# grab the last index in the indexes list and add the
		# index value to the list of picked indexes
		last = len(idxs) - 1
		i = idxs[last]
		pick.append(i)
 
		# find the largest (x, y) coordinates for the start of
		# the bounding box and the smallest (x, y) coordinates
		# for the end of the bounding box
		xx1 = np.maximum(x1[i], x1[idxs[:last]])
		yy1 = np.maximum(y1[i], y1[idxs[:last]])
		xx2 = np.minimum(x2[i], x2[idxs[:last]])
		yy2 = np.minimum(y2[i], y2[idxs[:last]])
 
		# compute the width and height of the bounding box
		w = np.maximum(0, xx2 - xx1 + 1)
		h = np.maximum(0, yy2 - yy1 + 1)
 
		# compute the ratio of overlap
		overlap = (w * h) / area[idxs[:last]]
 
		# delete all indexes from the index list that have
		idxs = np.delete(idxs, np.concatenate(([last],
			np.where(overlap > overlapThresh)[0])))
 
	# return only the bounding boxes that were picked using the
	# integer data type
	return boxes[pick].astype("int")

DETECT_OVERLAP_THRESH = 0.3
TRACK_OVERLAP_THRESH = 0.3
BOX_SCORE_THRESH = 3
LAST_DETECTION_THRESH = 25
TOTAL_DETECTIONS_THRESH = 5
TRIM_AMOUNT = 10

def collate_boxes(base, tr):
    gold = [[] for i in range(len(base))]
    tracks = []

    avg = 0
    for i in range(len(base)):
        base_boxes = base[i]
        tracked_boxes = tr[i]

        avg += len(base_boxes)
        avg += len(tracked_boxes)

        for box in base_boxes:
            # check for overlap
            overlapped = False
            for t in tracks:
                if iou(box, t['boxes'][-1]) > DETECT_OVERLAP_THRESH:
                    overlapped = True
                    if len(t['boxes']) + t['start'] > i:
                        t['boxes'][i - t['start']] = box
                    else:
                        t['last_detection'] = 0
                        t['total_detections'] += 1
                        t['boxes'].append(box)
                    break
            if overlapped:
                continue
            # else add new track if threshold high enough
            if box[4] > BOX_SCORE_THRESH:
                track = {
                    'start': i,
                    'start_box': box,
                    'boxes': [box],
                    'last_detection': 0,
                    'total_detections': 0,
                }
                tracks.append(track)

        dead_tracks = []
        for z, t in enumerate(tracks):
            t['last_detection'] += 1
            if t['last_detection'] > LAST_DETECTION_THRESH:
                # remove track
                if t['total_detections'] > TOTAL_DETECTIONS_THRESH:
                    print(t['total_detections'])
                    num_valid_frames = (
                        i - t['last_detection'] + TRIM_AMOUNT - t['start'])
                    for n in range(0, num_valid_frames):
                        gold[t['start'] + n].append(t['boxes'][n])
                dead_tracks.append(z)
                continue

            if len(t['boxes']) + t['start'] > i: continue
            overlapped = False
            for box in tracked_boxes:
                # check for overlap
                if iou(box, t['boxes'][-1]) > TRACK_OVERLAP_THRESH:
                    t['boxes'].append(box)
                    t['total_detections'] += 1
                    overlapped = True
                    break
            if not overlapped:
                t['boxes'].append(t['boxes'][-1])
                print('DID NOT OVERLAP')

        dead_tracks.reverse()
        for z in dead_tracks:
            tracks.pop(z)

    gold_avg = 0
    for boxes in gold:
        gold_avg += len(boxes)

    gold_avg /= 1.0 * len(base)
    avg /= 1.0 * len(base)
    print("gold", gold_avg)
    print("avg", avg)

    return gold


def main():
    dataset_name = "kcam_three"
    input_job = "facenet_k3"
    input_base_column = "base_bboxes"
    input_track_column = "tracked_bboxes"
    output_job = "gold_k3"
    output_column = "base_bboxes"

    #save_debug_video()
    meta = scanner.load_db_metadata()
    base_boxes = load_bboxes(dataset_name, input_job, input_base_column)
    tracked_boxes = load_bboxes(dataset_name, input_job, input_track_column)
    gold_boxes = []
    for base, tracked in zip(base_boxes, tracked_boxes):
        base_data = base["buffers"]
        tracked_data = tracked["buffers"]
        gold_data = collate_boxes(base_data, tracked_data)
        gold_data = [nms(boxes, 0.3) for boxes in gold_data]
        gold_boxes.append(gold_data)

    job_id = meta.next_job_id
    meta.next_job_id += 1
    save_bboxes(dataset_name, output_job, job_id, output_column, gold_boxes)
    job = meta.jobs.add()
    job.id = job_id
    job.name = output_job
    dataset_id = -1
    for dataset in meta.datasets:
        if dataset.name == dataset_name:
            dataset_id = dataset.id
            break;
    assert(dataset_id != -1)
    jtd = meta.job_to_datasets.add()
    jtd.job_id = job_id
    jtd.dataset_id = dataset_id

    scanner.write_db_metadata(meta)

if __name__ == "__main__":
    main()
