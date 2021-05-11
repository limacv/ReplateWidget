import os
cwd = os.path.dirname(os.path.realpath(__file__))
os.chdir(cwd)
from utils.datasets import VideoDataset
from utils.general import check_img_size
from utils.parser import get_config
from deep_sort import DeepSort
import argparse
import cv2
import torch

palette = (2 ** 11 - 1, 2 ** 15 - 1, 2 ** 20 - 1)


def bbox_rel(*xyxy):
    """" Calculates the relative bounding box from absolute pixel values. """
    bbox_left = min([xyxy[0].item(), xyxy[2].item()])
    bbox_top = min([xyxy[1].item(), xyxy[3].item()])
    bbox_w = abs(xyxy[0].item() - xyxy[2].item())
    bbox_h = abs(xyxy[1].item() - xyxy[3].item())
    x_c = (bbox_left + bbox_w / 2)
    y_c = (bbox_top + bbox_h / 2)
    w = bbox_w
    h = bbox_h
    return x_c, y_c, w, h


def compute_color_for_labels(label):
    """
    Simple function that adds fixed color depending on the class
    """
    color = [int((p * (label ** 2 - label + 1)) % 255) for p in palette]
    return tuple(color)


def draw_boxes(img, bbox, identities=None, offset=(0, 0)):
    for i, box in enumerate(bbox):
        x1, y1, x2, y2 = [int(i) for i in box]
        x1 += offset[0]
        x2 += offset[0]
        y1 += offset[1]
        y2 += offset[1]
        # box text and bar
        identy = int(identities[i]) if identities is not None else 0
        color = compute_color_for_labels(identy)
        label = '{}{:d}'.format("", identy)
        t_size = cv2.getTextSize(label, cv2.FONT_HERSHEY_PLAIN, 2, 2)[0]
        cv2.rectangle(img, (x1, y1), (x2, y2), color, 2)
        cv2.rectangle(
            img, (x1, y1), (x1 + t_size[0] + 3, y1 + t_size[1] + 4), color, -1)
        cv2.putText(img, label, (x1, y1 +
                                 t_size[1] + 4), cv2.FONT_HERSHEY_PLAIN, 2, [255, 255, 255], 2)
    return img


def track(opt):
    save_path, source, imgsz = opt.save_path, opt.source, opt.img_size

    # initialize deepsort
    cfg = get_config()
    cfg.merge_from_file(opt.config_deepsort)
    deepsorts = {}
    n_init = cfg.DEEPSORT.N_INIT
    # Set Dataloader
    dataset = VideoDataset(source, img_size=imgsz)

    vid_writer = None
    basename = os.path.basename(source).split('.')[0]
    txt_path = os.path.join(save_path, 'tracking.txt')
    vid_path = os.path.join(save_path, f'{basename}_tracking.mp4')

    # check whether detect output is there
    if not os.path.exists(save_path):
        raise RuntimeError("The detection folder corrupted, please rerun the detect.py")

    if opt.save_txt:
        with open(txt_path, 'w') as f:
            f.write("# frameidx, class, id, left, top, wid, hei\n")

    for frame_idx in range(-n_init, len(dataset)):
        path, img, im0s, vid_cap = dataset[abs(frame_idx)]
        # load the detection results from file
        detect_filename = os.path.join(save_path, f'{basename}_{abs(frame_idx)}.txt')
        if not os.path.exists(save_path):
            raise RuntimeError(f"The detection file {basename}_{abs(frame_idx)}.txt corrupted, "
                               f"please rerun the detect.py")

        gn = torch.tensor(im0s.shape)[[1, 0, 1, 0]]  # normalization gain whwh
        with open(detect_filename, 'r') as f:
            lines = f.readlines()[1:]  # remove the comment line
        lines = [list(map(float, line.split(' '))) for line in lines]
        det = torch.tensor(lines)
        img0 = im0s.copy()

        if len(det):
            # iterate through all the class
            classes = det[:, 5].int()
            unique = set(torch.unique(classes).tolist())
            for classid in unique:
                onedet = det[classes == classid, :5]

                bbox_xywh = onedet[:, :4] * gn
                confs = onedet[:, 4:5]

                if classid not in deepsorts.keys():
                    deepsorts[classid] = DeepSort(cfg)

                # Pass detections to deepsort
                outputs = deepsorts[classid].update(bbox_xywh, confs, img0)

                # draw boxes for visualization
                if len(outputs) > 0:
                    bbox_xyxy = outputs[:, :4]
                    identities = outputs[:, -1]

                    draw_boxes(img0, bbox_xyxy, identities)

                # Write MOT compliant results to file
                if opt.save_txt and len(outputs) != 0 and frame_idx >= 0:
                    with open(txt_path, 'a') as f:
                        for j, output in enumerate(outputs):
                            bbox_left = output[0]
                            bbox_top = output[1]
                            bbox_w = output[2] - output[0]
                            bbox_h = output[3] - output[1]
                            identity = output[-1]
                            f.write(('%g ' * 7 + '\n') % (frame_idx, classid, identity, bbox_left,
                                                          bbox_top, bbox_w, bbox_h))  # label format
            for classid in set(deepsorts.keys()) - unique:
                deepsorts[classid].increment_ages()
        else:
            for deepsort in deepsorts.values():
                deepsort.increment_ages()

        # Save results (image with detections)
        if opt.save_vid and frame_idx >= 0:
            if dataset.mode == 'images':
                cv2.imwrite(save_path, img0)
                raise NotImplementedError("images not implemented")
            else:
                if vid_writer is None or not vid_writer.isOpened():
                    fps, w, h = 30, img0.shape[1], img0.shape[0]
                    vid_writer = cv2.VideoWriter(
                        vid_path, cv2.VideoWriter_fourcc(*'mp4v'), fps, (w, h))

                vid_writer.write(img0)

    if opt.save_txt:
        print('Results saved to %s' % os.getcwd() + os.sep + save_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=str,
                        default='inference/images', help='source')
    parser.add_argument('--save_path', type=str, default='exampleoutput',
                        help='output folder')  # output folder
    parser.add_argument('--img-size', type=int, default=1280,
                        help='inference size (pixels)')
    parser.add_argument('--device', default='0',
                        help='cuda device, i.e. 0 or 0,1,2,3 or cpu')
    parser.add_argument('--save-txt', default=True, action='store_true',
                        help='save results to *.txt')
    parser.add_argument('--save-vid', default=True, action='store_true',
                        help='save results to *_tracking.mp4')
    parser.add_argument('--classes', nargs='+', type=int,
                        default=[0], help='filter by class')
    parser.add_argument("--config_deepsort", type=str,
                        default="deep_sort/deep_sort.yaml")
    args = parser.parse_args()
    args.img_size = check_img_size(args.img_size)

    with torch.no_grad():
        track(args)
