import torch
import torch.nn.functional as F
import numpy as np
import random, time, math, os, glob, io, gzip

from network import Network
from data import Data

from torch.nn import DataParallel
from lazy_loader import LazyLoader, LoaderFlag
from status_loader import StatusLoader

def gather_filenames(root, num_chunks=None, sort_key_fn=None):
    def gather_recursive_files(root):
        l = list()
        for name in glob.glob(os.path.join(root, "*")):
            if os.path.isdir(name):
                l.extend(gather_recursive_files(name))
            else:
                l.append(name)
        return l
    chunks = gather_recursive_files(root)

    if num_chunks is not None:
        if len(chunks) > num_chunks:
            if sort_key_fn is None:
                random.shuffle(chunks)
            else:
                chunks.sort(key=sort_key_fn, reverse=True)
            chunks = chunks[:num_chunks]
    return chunks

class StreamLoader:
    def __init__(self):
        pass

    def func(self, filename):
        stream = None
        if not os.path.isfile(filename):
            return stream

        try:
            if filename.find(".gz") >= 0:
                with gzip.open(filename, 'rt') as f:
                    stream = io.StringIO(f.read())
            else:
                with open(filename, 'r') as f:
                    stream = io.StringIO(f.read())
        except:
            print("Could not open the file: {}".format(filename))
        return stream

class StreamParser:
    def __init__(self, down_sample_rate):
        # Use a random sample input data read. This helps improve the spread of
        # games in the shuffle buffer.
        self.down_sample_rate = down_sample_rate

    def func(self, stream):
        if stream is None:
            return None
        data = Data()

        while True:
            skip_this_time = False
            if self.down_sample_rate > 1:
                if random.randint(0, self.down_sample_rate-1) != 0:
                    skip_this_time = True

            success = data.parse_from_stream(stream, skip_this_time)

            if success == False:
                return None # stream is end
            if skip_this_time == False:
                break

        data.apply_symmetry(random.randint(0, 7))
        return data

class BatchGenerator:
    def __init__(self, boardsize, input_channels):
        self.nn_board_size = boardsize
        self.nn_num_intersections = self.nn_board_size * self.nn_board_size
        self.input_channels = input_channels

    def _wrap_data(self, data):
        nn_board_size = self.nn_board_size
        nn_num_intersections = self.nn_num_intersections

        board_size = data.board_size
        num_intersections = data.board_size * data.board_size

        # allocate all buffers
        input_planes = np.zeros((self.input_channels, nn_board_size, nn_board_size))
        prob = np.zeros(nn_num_intersections+1)
        aux_prob = np.zeros(nn_num_intersections+1)
        ownership = np.zeros((nn_board_size, nn_board_size))
        wdl = np.zeros(3)
        all_q_vals = np.zeros(5)
        all_scores = np.zeros(5)

        buf = np.zeros(num_intersections)
        sqr_buf = np.zeros((nn_board_size, nn_board_size))

        # input planes
        for p in range(self.input_channels-6):
            plane = data.planes[p]
            input_planes[p, 0:board_size, 0:board_size] = np.reshape(plane, (board_size, board_size))[:, :]

        input_planes[self.input_channels-6, 0:board_size, 0:board_size] = data.rule
        input_planes[self.input_channels-5, 0:board_size, 0:board_size] = data.wave
        if data.to_move == 1:
            input_planes[self.input_channels-4, 0:board_size, 0:board_size] =  data.komi/20
            input_planes[self.input_channels-3, 0:board_size, 0:board_size] = -data.komi/20
        else:
            input_planes[self.input_channels-4, 0:board_size, 0:board_size] = -data.komi/20
            input_planes[self.input_channels-3, 0:board_size, 0:board_size] =  data.komi/20
        input_planes[self.input_channels-2, 0:board_size, 0:board_size] = (data.board_size**2)/361
        input_planes[self.input_channels-1, 0:board_size, 0:board_size] = 1 # fill ones

        # probabilities
        buf[:] = data.prob[0:num_intersections]
        sqr_buf[0:board_size, 0:board_size] = np.reshape(buf, (board_size, board_size))[:, :]
        prob[0:nn_num_intersections] = np.reshape(sqr_buf, (nn_num_intersections))[:]
        prob[nn_num_intersections] = data.prob[num_intersections]

        # auxiliary probabilities
        buf[:] = data.aux_prob[0:num_intersections]
        sqr_buf[0:board_size, 0:board_size] = np.reshape(buf, (board_size, board_size))[:, :]
        aux_prob[0:nn_num_intersections] = np.reshape(sqr_buf, (nn_num_intersections))[:]
        aux_prob[nn_num_intersections] = data.aux_prob[num_intersections]

        # ownership
        ownership[0:board_size, 0:board_size] = np.reshape(data.ownership, (board_size, board_size))[:, :]
        ownership = np.reshape(ownership, (nn_num_intersections))

        # winrate
        wdl[1 - data.result] = 1

        # all q values
        all_q_vals[0] = data.result
        all_q_vals[1] = data.avg_q
        all_q_vals[2] = data.short_avg_q
        all_q_vals[3] = data.mid_avg_q
        all_q_vals[4] = data.long_avg_q

        # all scores
        all_scores[0] = data.final_score
        all_scores[1] = data.avg_score
        all_scores[2] = data.short_avg_score
        all_scores[3] = data.mid_avg_score
        all_scores[4] = data.long_avg_score

        return (
            input_planes,
            prob,
            aux_prob,
            ownership,
            wdl,
            all_q_vals,
            all_scores
        )

    def func(self, data_list):
        batch_planes = list()
        batch_prob = list()
        batch_aux_prob = list()
        batch_ownership = list()
        batch_wdl = list()
        batch_q_vals = list()
        batch_scores = list()

        for data in data_list:
            planes, prob, aux_prob, ownership, wdl, q_vals, scores = self._wrap_data(data)

            batch_planes.append(planes)
            batch_prob.append(prob)
            batch_aux_prob.append(aux_prob)
            batch_ownership.append(ownership)
            batch_wdl.append(wdl)
            batch_q_vals.append(q_vals)
            batch_scores.append(scores)

        batch_dict = {
            "planes"        : torch.from_numpy(np.array(batch_planes)).float(),
            "prob"          : torch.from_numpy(np.array(batch_prob)).float(),
            "aux_prob"      : torch.from_numpy(np.array(batch_aux_prob)).float(),
            "ownership"     : torch.from_numpy(np.array(batch_ownership)).float(),
            "wdl"           : torch.from_numpy(np.array(batch_wdl)).float(),
            "q_vals"        : torch.from_numpy(np.array(batch_q_vals)).float(),
            "scores"        : torch.from_numpy(np.array(batch_scores)).float()
        }
        return batch_dict

class TrainingPipe():
    def __init__(self, cfg):
        self.cfg = cfg

        # The mini-batch size, update the network per batch size
        self.batchsize =  cfg.batchsize

        # The marco batch size and factor, (marco batch size) * factor = batch size
        self.macrobatchsize = cfg.macrobatchsize
        self.macrofactor = cfg.macrofactor

        # How many cpu does the 'DataLoader' use?
        self.num_workers = cfg.num_workers
        self.train_dir = cfg.train_dir
        self.validation_dir = cfg.validation_dir

        # How many last chunks do we load?
        self.num_chunks = cfg.num_chunks

        # The stpes of storing the last model and validating it per epoch.
        self.steps_per_epoch =  cfg.steps_per_epoch

        # Report the information per this steps.
        self.verbose_steps = max(100, cfg.verbose_steps)

        self.validation_steps = cfg.validation_steps

        # Lazy loader options.
        self.train_buffer_size = self.cfg.buffersize
        self.validation_buffer_size = self.cfg.buffersize // 10
        self.down_sample_rate = cfg.down_sample_rate

        # Max steps per training task.
        self.max_steps =  cfg.max_steps

        # Which optimizer do we use?
        self.opt_name = cfg.optimizer

        # Optimizer's parameters.
        self.weight_decay = cfg.weight_decay
        self.lr_schedule = cfg.lr_schedule

        # The training device.
        self.use_gpu = cfg.use_gpu
        self.device = torch.device("cpu")
        if self.use_gpu:
            self.device = torch.device("cuda")
        self.net = Network(cfg)
        self.net.train()

        self.swa_net = Network(cfg)
        self.swa_net.eval()

        # The Store root directory.
        self.store_path = cfg.store_path
        self._status_loader = StatusLoader()

        # The SWA setting.
        self.swa_count = 0
        self.swa_max_count = self.cfg.swa_max_count
        self.swa_steps = self.cfg.swa_steps

        self._setup()

    def _setup(self):
        self.module = self.net # linking

        if self.use_gpu:
            self.net = self.net.to(self.device)
            self.net = DataParallel(self.net) 
            self.module  = self.net.module
            self.swa_net = self.swa_net.to(self.device)

        # Copy the initial weights.
        self.swa_net.accumulate_swa(self.module, 0)

        init_lr = self._get_lr_schedule(0)

        # We may fail to load the optimizer. So initializing
        # it before loading it.
        self.opt = None
        if self.opt_name == "Adam":
            self.opt = torch.optim.Adam(
                self.net.parameters(),
                lr=init_lr,
                weight_decay=self.weight_decay,
            )
        elif self.opt_name == "SGD":
            # Recommanded optimizer, the SGD is better than Adam
            # in this kind of training task.
            self.opt = torch.optim.SGD(
                self.net.parameters(),
                lr=init_lr,
                momentum=0.9,
                nesterov=True,
                weight_decay=self.weight_decay,
            )

        self.weights_path = os.path.join(self.store_path, "weights")
        if not os.path.isdir(self.weights_path):
            os.mkdir(self.weights_path)

        self.swa_weights_path = os.path.join(self.store_path, "swa")
        if not os.path.isdir(self.swa_weights_path):
            os.mkdir(self.swa_weights_path)

        info_file = os.path.join(self.store_path, "info.txt")
        with open(info_file, 'w') as f:
            f.write(self.module.simple_info())

        self._loss_weight_dict = {
            "soft" : self.cfg.soft_loss_weight
        }

    def _get_lr_schedule(self, num_steps):
        # Get the current learning rate from schedule.
        curr_lr = 0.2
        for s, lr in self.lr_schedule:
            if s <= num_steps:
                curr_lr = lr
            else:
                break
        return curr_lr

    def _load_current_status(self):
        status_name = os.path.join(self.store_path, "last_status.pt")
        self._status_loader.load(status_name, device=torch.device("cpu"))
        self._status_loader.load_model(self.module)
        self._status_loader.load_swa_model(self.swa_net)
        self._status_loader.load_optimizer(self.opt)

        last_steps = self._status_loader.get_steps()
        self.module.update_parameters(last_steps)

        self.swa_count = self._status_loader.get_swa_count()
        curr_lr = self._get_lr_schedule(last_steps)

        for param in self.opt.param_groups:
            param["lr"] = curr_lr
            param["weight_decay"] = self.weight_decay

        print("Current steps is {}, learning rate is {}.".format(last_steps, curr_lr))

        return last_steps

    def _save_current_status(self, steps):
        status_name = os.path.join(self.store_path, "last_status.pt")

        self.validate_the_last_model(steps)

        self._status_loader.set_steps(steps)
        self._status_loader.set_swa_count(self.swa_count)
        self._status_loader.save_model(self.module)
        self._status_loader.save_swa_model(self.swa_net)
        self._status_loader.save_optimizer(self.opt)
        self._status_loader.save(status_name)

        weights_name = os.path.join(self.weights_path, "s{}.bin.txt".format(steps))
        cpu_module = self.module.to("cpu")
        cpu_module.transfer_to_bin(weights_name)

        swa_weights_name = os.path.join(self.swa_weights_path, "swa-s{}.bin.txt".format(steps))
        cpu_swa_net = self.swa_net.to("cpu")
        cpu_swa_net.transfer_to_bin(swa_weights_name)

        if self.use_gpu:
            self.module = self.module.to(self.device)
            self.swa_net = self.swa_net.to(self.device)

    def _init_loader(self):
        self._stream_loader = StreamLoader()
        self._stream_parser = StreamParser(self.down_sample_rate)
        self._batch_gen = BatchGenerator(self.cfg.boardsize, self.cfg.input_channels)

        sort_fn = os.path.getmtime
        chunks = gather_filenames(self.train_dir, self.num_chunks, sort_fn)

        print("Load the last {} chunks...".format(len(chunks)))

        self.flag = LoaderFlag()
        self.train_lazy_loader = LazyLoader(
            filenames = chunks,
            stream_loader = self._stream_loader,
            stream_parser = self._stream_parser,
            batch_generator = self._batch_gen,
            down_sample_rate = 0,
            num_workers = self.num_workers,
            buffer_size = self.train_buffer_size,
            batch_size = self.macrobatchsize,
            flag = self.flag
        )

        # Try to get the first batch, be sure that the loader is ready.
        batch = next(self.train_lazy_loader)

        if self.validation_dir is not None:
            self.validation_lazy_loader = LazyLoader(
                filenames = gather_filenames(self.validation_dir, len(chunks)//10, sort_fn),
                stream_loader = self._stream_loader,
                stream_parser = self._stream_parser,
                batch_generator = self._batch_gen,
                down_sample_rate = 0,
                num_workers = self.num_workers,
                buffer_size = self.validation_buffer_size,
                batch_size = self.macrobatchsize,
                flag = self.flag
            )
            batch = next(self.validation_lazy_loader)
        else:
            self.validation_lazy_loader = None

    def _get_new_running_loss_dict(self, all_loss_dict):
        # Get the new dict.
        running_loss_dict = dict()
        running_loss_dict["loss"] = 0
        for k, v in all_loss_dict.items():
            running_loss_dict[k] = 0
        return running_loss_dict

    def _accumulate_loss(self, running_loss_dict, all_loss_dict):
        for k, v in all_loss_dict.items():
            this_item_loss = v.item()
            running_loss_dict[k] += this_item_loss
            running_loss_dict["loss"] += this_item_loss
        return running_loss_dict

    def _handle_loss(self, all_loss_dict):
        for k, v in all_loss_dict.items():
            v /= self.macrofactor

        loss = all_loss_dict["prob_loss"]
        for k, v in all_loss_dict.items():
            if k == "prob_loss":
               continue
            loss += v
        return loss, all_loss_dict

    def _get_loss_info(self, running_loss_dict):
        info = str()
        info += "\tloss: {:.4f}\n".format(running_loss_dict["loss"]/self.verbose_steps)
        info += "\tprob loss: {:.4f}\n".format(running_loss_dict["prob_loss"]/self.verbose_steps)
        info += "\taux prob loss: {:.4f}\n".format(running_loss_dict["aux_prob_loss"]/self.verbose_steps)
        info += "\tsoft prob loss: {:.4f}\n".format(running_loss_dict["soft_prob_loss"]/self.verbose_steps)
        info += "\tsoft aux prob loss: {:.4f}\n".format(running_loss_dict["soft_aux_prob_loss"]/self.verbose_steps)
        info += "\toptimistic loss: {:.4f}\n".format(running_loss_dict["optimistic_loss"]/self.verbose_steps)
        info += "\townership loss: {:.4f}\n".format(running_loss_dict["ownership_loss"]/self.verbose_steps)
        info += "\twdl loss: {:.4f}\n".format(running_loss_dict["wdl_loss"]/self.verbose_steps)
        info += "\tQ values loss: {:.4f}\n".format(running_loss_dict["q_vals_loss"]/self.verbose_steps)
        info += "\tscores loss: {:.4f}\n".format(running_loss_dict["scores_loss"]/self.verbose_steps)
        info += "\terrors loss: {:.4f}".format(running_loss_dict["errors_loss"]/self.verbose_steps)
        return info

    def _gather_data_from_loader(self, use_training=True):
        # Fetch the next batch data from disk.
        if use_training:
            batch_dict = next(self.train_lazy_loader)
        else:
            batch_dict = next(self.validation_lazy_loader)

        # Move the data to the current device.
        if self.use_gpu:
            for k, v in batch_dict.items():
                v = v.to(self.device)

        # Gather batch data
        planes = batch_dict["planes"]
        target = (
            batch_dict["prob"],
            batch_dict["aux_prob"],
            batch_dict["ownership"],
            batch_dict["wdl"],
            batch_dict["q_vals"],
            batch_dict["scores"]
        )
        return planes, target

    def validate_the_last_model(self, steps):
        pass

    def fit_and_store(self):
        init_steps = self._load_current_status()

        self._init_loader()
        print("Start training...")

        running_loss_dict = dict()
        num_steps = init_steps
        keep_running = True
        macro_steps = 0

        clock_time = time.time()

        while keep_running:
            for _ in range(self.steps_per_epoch):
                planes, target = self._gather_data_from_loader(True)

                # forward and backforwad
                _, all_loss_dict = self.net(
                    planes,
                    target,
                    use_symm=True,
                    loss_weight_dict=self._loss_weight_dict
                )

                # compute loss
                loss, all_loss_dict = self._handle_loss(all_loss_dict)
                loss.backward()
                macro_steps += 1

                # accumulate loss
                if len(running_loss_dict) == 0:
                    running_loss_dict = self._get_new_running_loss_dict(all_loss_dict)
                running_loss_dict = self._accumulate_loss(running_loss_dict, all_loss_dict)
 
                if math.isnan(running_loss_dict["loss"]):
                    print("The gradient is explosion. Stop the training...")
                    keep_running = False
                    break

                if macro_steps % self.macrofactor == 0:
                    # clip grad
                    if self.cfg.fixup_batch_norm:
                        gnorm = torch.nn.utils.clip_grad_norm_(self.net.parameters(), 400.0)

                    # update network
                    self.opt.step()
                    self.opt.zero_grad()

                    num_steps += 1
                    self.module.update_parameters(num_steps)

                    # dump the verbose
                    if num_steps % self.verbose_steps == 0:
                        elapsed = time.time() - clock_time
                        clock_time = time.time()

                        log_outs = "steps: {} -> ".format(num_steps)
                        log_outs += "speed: {:.2f}, opt: {}, learning rate: {}, batch size: {}\n".format(
                                        self.verbose_steps/elapsed,
                                        self.opt_name,
                                        self.opt.param_groups[0]["lr"],
                                        self.batchsize)
                        log_outs += self._get_loss_info(running_loss_dict)
                        print(log_outs)

                        # Also save the current running loss.
                        log_file = os.path.join(self.store_path, "training.log")
                        with open(log_file, 'a') as f:
                            f.write(log_outs + '\n')
                        running_loss_dict = self._get_new_running_loss_dict(all_loss_dict)


                    if num_steps % self.swa_steps == 0:
                        self.swa_count = min(self.swa_count+1, self.swa_max_count)
                        self.swa_net.accumulate_swa(self.module, self.swa_count)

                    # update learning rate
                    for param in self.opt.param_groups:
                        param["lr"] = self._get_lr_schedule(num_steps) 

                # should we stop it?
                if num_steps >= self.max_steps + init_steps:
                    keep_running = False
                    break

            # store the last network
            self._save_current_status(num_steps)
        self.flag.set_stop_flag()
        try:
            planes, target = self._gather_data_from_loader(True)
        except StopIteration:
            pass
        print("Training is over.")
