import os
cwd = os.path.dirname(os.path.realpath(__file__))
os.chdir(cwd)
import argparse

import cv2
import torch
from numpy import random

from utils.datasets import LoadImages
from utils.general import non_max_suppression, check_img_size, scale_coords, xyxy2xywh
from utils.plots import plot_one_box
from utils.torch_utils import select_device


def detect(opt):
    source, weights, save_txt, imgsz = opt.source, opt.weights, opt.save_txt, opt.img_size
    save_img = not opt.nosave and not source.endswith('.txt')  # save info

    # Directories
    os.makedirs(opt.save_path, exist_ok=True)

    # Initialize
    device = select_device(opt.device)
    half = device.type != 'cpu'  # half precision only supported on CUDA

    # Load model
    model = torch.load(weights, map_location=device)['model'].float()
    imgsz = check_img_size(imgsz)  # check img_size
    if half:
        model.half()  # to FP16

    # Set Dataloader
    vid_path, vid_writer = None, None
    dataset = LoadImages(source, img_size=imgsz)

    # Get names and colors
    names = model.module.names if hasattr(model, 'module') else model.names
    colors = [[random.randint(0, 255) for _ in range(3)] for _ in names]

    # Run inference
    if device.type != 'cpu':
        model(torch.zeros(1, 3, imgsz, imgsz).to(device).type_as(next(model.parameters())))  # run once
    for path, img, im0s, vid_cap in dataset:
        img = torch.from_numpy(img).to(device)
        img = img.half() if half else img.float()  # uint8 to fp16/32
        img /= 255.0  # 0 - 255 to 0.0 - 1.0
        if img.ndimension() == 3:
            img = img.unsqueeze(0)

        # Inference
        pred = model(img)[0]

        # Apply NMS
        pred = non_max_suppression(pred, opt.conf_thres, opt.iou_thres, classes=opt.classes, agnostic=opt.agnostic_nms)

        # Process detections
        for i, det in enumerate(pred):  # detections per image
            p, s, im0, frame = path, '', im0s.copy(), getattr(dataset, 'frame', 0)

            save_path = os.path.join(opt.save_path, os.path.basename(p))  # img.jpg
            txt_path = os.path.join(opt.save_path, os.path.basename(p).split('.')[0]) \
                       + ('' if dataset.mode == 'image' else f'_{frame - 1}')\
                       + '.txt'  # img.txt
            if save_txt:
                with open(txt_path, 'w') as f:
                    f.write('# centerx, centery, wid, hei, confidence, class\n')
            gn = torch.tensor(im0.shape)[[1, 0, 1, 0]]  # normalization gain whwh
            if len(det):
                # Rescale boxes from img_size to im0 size
                det[:, :4] = scale_coords(img.shape[2:], det[:, :4], im0.shape).round()

                # Write results
                for *xyxy, conf, cls in reversed(det):
                    if save_txt:  # Write to file
                        xywh = (xyxy2xywh(torch.tensor(xyxy).view(1, 4)) / gn).view(-1).tolist()  # normalized xywh
                        line = (*xywh, conf, cls) if opt.save_conf else (cls, *xywh)  # label format
                        with open(txt_path, 'a') as f:
                            f.write(('%g ' * len(line)).rstrip() % line + '\n')

                    if save_img:  # Add bbox to image
                        c = int(cls)  # integer class
                        label = None if opt.hide_labels else (names[c] if opt.hide_conf else f'{names[c]} {conf:.2f}')

                        plot_one_box(xyxy, im0, label=label, color=colors[c], line_thickness=opt.line_thickness)

            # Save results (image with detections)
            if save_img:
                if dataset.mode == 'image':
                    cv2.imwrite(save_path, im0)
                    raise NotImplementedError("images not implemented")
                else:  # 'video' or 'stream'
                    if vid_path != save_path:  # new video
                        vid_path = save_path
                        if isinstance(vid_writer, cv2.VideoWriter):
                            vid_writer.release()  # release previous video writer
                        if vid_cap:  # video
                            fps = vid_cap.get(cv2.CAP_PROP_FPS)
                            w = int(vid_cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                            h = int(vid_cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                        else:  # stream
                            fps, w, h = 30, im0.shape[1], im0.shape[0]
                            save_path += '.mp4'
                        vid_writer = cv2.VideoWriter(save_path, cv2.VideoWriter_fourcc(*'mp4v'), fps, (w, h))
                    vid_writer.write(im0)

    if save_txt or save_img:
        print(f"Results saved to {opt.save_path}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--weights', type=str, default='yolov5m.pt', help='model.pt path(s)')
    parser.add_argument('--source', type=str, help='source')  # file/folder, 0 for webcam
    parser.add_argument('--save_path', default='exampleoutput', help='save results to name')
    parser.add_argument('--img-size', type=int, default=1280, help='inference size (pixels)')
    parser.add_argument('--conf-thres', type=float, default=0.3, help='object confidence threshold')
    parser.add_argument('--iou-thres', type=float, default=0.7, help='IOU threshold for NMS')
    parser.add_argument('--device', default='0', help='cuda device, i.e. 0 or 0,1,2,3 or cpu')
    parser.add_argument('--save-txt', default=True, action='store_true', help='save results to *.txt')
    parser.add_argument('--save-conf', default=True, action='store_true', help='save confidences in --save-txt labels')
    parser.add_argument('--nosave', action='store_true', help='do not save images/videos')
    parser.add_argument('--classes', nargs='+', type=int, help='filter by class: --class 0, or --class 0 2 3')
    parser.add_argument('--agnostic-nms', action='store_true', help='class-agnostic NMS')
    parser.add_argument('--line-thickness', default=2, type=int, help='bounding box thickness (pixels)')
    parser.add_argument('--hide-labels', default=False, action='store_true', help='hide labels')
    parser.add_argument('--hide-conf', default=False, action='store_true', help='hide confidences')
    opt = parser.parse_args()

    with torch.no_grad():
        detect(opt=opt)
