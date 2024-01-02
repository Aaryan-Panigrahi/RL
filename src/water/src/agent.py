import os
import time
import numpy as np
import random
import json

import torch
import torch.nn as nn
import torch.optim as optim

class model(nn.Module):

    def __init__(self, orientation_length, scan_length, n_actions):
        
        super(model, self).__init__()
        leaky_slope = 0.0

        self.scan_network = nn.Sequential(
            nn.Conv1d(in_channels=1, out_channels=4, kernel_size=4, padding=1),
            nn.LeakyReLU(leaky_slope),

            nn.Flatten(),

            nn.Linear(4 * scan_length - 4, 128),
            nn.LeakyReLU(leaky_slope),

            nn.Linear(128, 64),
            nn.LeakyReLU(leaky_slope)
        )

        self.orientation_network = nn.Sequential(
            nn.Linear(orientation_length, 64),
            nn.LeakyReLU(leaky_slope),

            nn.Linear(64, 128),
            nn.LeakyReLU(leaky_slope),

            nn.Linear(128, 64),
            nn.LeakyReLU(leaky_slope)
        )

        self.main_network = nn.Sequential(
            nn.Linear(128, 128),
            nn.LeakyReLU(leaky_slope),

            nn.Linear(128, n_actions),
        )

    def forward(self, orientation_data, scan_data):

        orientation0 = self.orientation_network(orientation_data)
        scan0 = self.scan_network(scan_data)

        main = torch.cat([orientation0, scan0], dim=1)
        Q_vals = self.main_network(main)

        return Q_vals

class actions():

    max_linear_vel = None
    min_linear_vel = None

    max_angular_vel = None
    min_angular_vel = None

    linear_vel_buckets = None
    angular_vel_buckets = None

    activity = dict()

    def update():

        actions.activity = {}
        count = 0
        V = np.linspace(
                actions.min_linear_vel, 
                actions.max_linear_vel,
                actions.linear_vel_buckets)
        W = np.linspace(
                actions.min_angular_vel,
                actions.max_angular_vel,
                actions.angular_vel_buckets)
        for v in V:
            for w in W:
                actions.activity[count] = (v, w)
                count = count + 1

    def save(json_file_name):
        
        with open(json_file_name, "w") as f:
            json.dump(actions.activity, f, indent=4)

    def get(action):

        return actions.activity[action]

class state():

    def __init__(self, orientation=None, scan=None):

        self.orientation = orientation
        self.scan = scan
        self.floated = False

    def floatify(self):
        if (self.floated):
            return
        self.orientation = torch.tensor([self.orientation], dtype=torch.float32)
        self.scan = torch.tensor([[self.scan]], dtype=torch.float32)
        self.floated = True

class agent():

    def __init__(self,
            orientation_length,
            scan_length,
            n_actions,
            alpha=0.001,
            gamma=0.9,
            epsilon=0.1,
            reuse=False):

        self.alpha = alpha
        self.gamma = gamma
        self.epsilon = epsilon

        self.orientation_length = orientation_length
        self.scan_length = scan_length
        self.n_actions = n_actions

        self.main_model = model(orientation_length, scan_length, n_actions)
        if (reuse):
            self.main_model.load_state_dict(torch.load(reuse))

        self.temp_model = model(orientation_length, scan_length, n_actions)
        self.temp_model.load_state_dict(self.main_model.state_dict())

        self.optimizer = optim.Adam(self.temp_model.parameters(), lr=alpha)
        self.loss_criteria = nn.MSELoss()

    def choose_action(self, state, echo=False):

        if (torch.rand(1).item() < self.epsilon):
            return torch.randint(0, self.n_actions, (1, )).item()

        state.floatify()

        with torch.no_grad():
            Q_values = self.temp_model(state.orientation, state.scan)
            action = Q_values.argmax().item()

        if (echo):
            print(q_values)

        return action

    def learn(self, state, new_state, action, reward, terminal):

        state.floatify()
        new_state.floatify()

        reward = torch.tensor(reward, dtype=torch.float32)
        current_Q_values = self.temp_model(state.orientation, state.scan)

        with torch.no_grad():
            next_Q_values = self.main_model(new_state.orientation, new_state.scan)
            maximum_future_Q = next_Q_values.max()
            target_Q_values = current_Q_values.clone()
            target_Q_values[0, action] = reward + self.gamma * (1 - terminal) * maximum_future_Q

        loss = self.loss_criteria(current_Q_values, target_Q_values)
        self.optimizer.zero_grad()
        loss.backward()
        self.optimizer.step()

    def update_main_model(self):
        self.main_model.load_state_dict(self.temp_model.state_dict())

    def save(self, file_name):
        torch.save(self.main_model.state_dict(), file_name)

