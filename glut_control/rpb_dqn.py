import rl_utils
from resolution_prebuffer import res_prebuffer, gnn_model
import numpy as np
from rl_dataset import simple_graph_dataset
from spektral.data import DisjointLoader
from tensorflow import keras
import tensorflow as tf
import pickle


class rpb_dqn(res_prebuffer):
    def __init__(self, subjects=[], words=[], use_tf=False, debug=True, use_gnn=True, gnn_nodes=20,
                 seed=0, gamma=0.99, epsilon=1.0, eps_min=0.1, eps_max=1.0, batch_size=16):
        super().__init__(subjects, words, use_tf, debug, use_gnn, gnn_nodes)
        self.seed=seed
        self.gamma=gamma
        self.epsilon=epsilon
        self.eps_min = eps_min=0.1
        self.eps_max=eps_max
        self.batch_size = batch_size
        self.current_model = self.model
        self.target_model = gnn_model()
        #self.loss_fn = keras.losses.Huber()
        self.loss_fn = keras.losses.MeanSquaredError()
        self.optimizer = keras.optimizers.Adam(learning_rate=0.00025, clipnorm=1.0)
        #self.acc_fn = CategoricalAccuracy()

    def get_priorities(self, inputs, current_model=True, training=False, numpy=True):
        X = self.vectorize(inputs)
        model = self.current_model if current_model else self.target_model
        dataset = simple_graph_dataset(X)
        loader = DisjointLoader(dataset, batch_size = self.batch_size)
        preds = []
        num_epochs = len(X) // self.batch_size
        if len(X) % self.batch_size != 0:
            num_epochs += 1    # Extra batch for remainder
        for i in range(num_epochs):
            b = loader.__next__()
            # b is now a triple:  b[0] is a batch of X, b[1] is a batch of A (as a SparseTensor),
            # b[2] is is some kind of index array
            batch_preds = model.graph_net(b, training=training)
            preds.append(batch_preds)
        if numpy:
            result = np.concatenate([p.numpy() for p in preds]).reshape(-1)
        else:
            result = tf.concat(axis=0, values = preds)
        return result


    def fit(self, batch, verbose=True):
        X = self.vectorize(batch.actions)
        batch_size = len(X)
        #dataset = simple_graph_dataset(X,np.zeros(len(X)))
        dataset = simple_graph_dataset(X)
        with tf.GradientTape() as tape:
            loader = DisjointLoader(dataset, batch_size = batch_size)
            preds = []
            total_loss = 0
            total_acc = 0
            b = loader.__next__()
            #inputs = (b[0], b[1], b[3])
            inputs = b
            future_rewards = self.target_model.graph_net(inputs, training=False)
            updated_q_values = np.array(batch.rewards).reshape(-1) + self.gamma * future_rewards
            q_values = self.current_model.graph_net(inputs, training=True)
            #updated_q_values = q_values + 1
            loss = self.loss_fn(updated_q_values, q_values)

            # Backpropagation
            grads = tape.gradient(loss, self.current_model.graph_net.trainable_variables)
            self.optimizer.apply_gradients(zip(grads, self.current_model.graph_net.trainable_variables))
        return history_struct({'accuracy': [np.nan],
                               'loss': [(loss / batch_size)]})

    def train_on_given_batch(self, inputs, target):
        """
        Note:  we explore using the target model (i.e. generate priorities) but train the current model. 
        TODO:  not working; use fit instead.
        """
        #future_rewards = network.target_model.predict(batch.actions)
        future_rewards = self.get_priorities(batch.actions, current_model=False, numpy=False)  # Use target_model

        # The original (below) probably isn't right for our network structure.   
        #updated_q_values = batch.rewards + network.gamma * tf.reduce_max(   future_rewards, axis=1 )
        updated_q_values = np.array(batch.rewards).reshape(-1) + self.gamma * future_rewards

        # If final frame set the last value to -1.
        # TODO:  Not sure how to translate this?  For now let's not use it; we'll
        # consider our episodes to be potentially infinite.  This will probably come in if
        # we want to limit the depth of the inference tree.
        #updated_q_values = updated_q_values * (1 - batch.done) - batch.done

        # Create a mask so we only calculate loss on the updated Q-values
        #masks = tf.one_hot(batch.actions, num_actions)


        # Train the model on the states and updated Q-values
        q_values = self.get_priorities(batch.actions, current_model=True, training=True, numpy=False)
        temporary_test_q = q_values + 1
        # Apply the masks to the Q-values to get the Q-value for action taken
        #q_action = tf.reduce_sum(tf.multiply(q_values, masks), axis=1)
        # Calculate loss between new Q-value and old Q-value
        #loss = network.loss_fn(updated_q_values, q_action)
        #loss = self.loss_fn(updated_q_values, q_values)
        loss = self.loss_fn(temporary_test_q, q_values)

        with tf.GradientTape() as tape:
            # Backpropagation
            grads = tape.gradient(loss, self.current_model.graph_net.trainable_variables)
            self.optimizer.apply_gradients(zip(grads, self.current_model.graph_net.trainable_variables))

    
    def model_save(self, id_str):
        pkl_file = open("rpb_dqn_model_{}.pkl".format(id_str), "wb")
        pickle.dump((self.subjects, self.words, self.num_subjects, self.num_words, self.subjects_dict, self.words_dict, self.batch_size, self.use_tf,
                     self.seed, self.gamma, self.epsilon, self.eps_min, self.eps_max, self.batch_size), pkl_file)
        self.current_model.graph_net.save("rpb_dqn_current_model_{}".format(id_str))
        self.target_model.graph_net.save("rpb_dqn_target_model_{}".format(id_str))

    def model_load(self, id_str):
        pkl_file = open("rpb_dqn_model_{}.pkl".format(id_str), "rb")
        (self.subjects, self.words, self.num_subjects, self.num_words, self.subjects_dict, self.words_dict,
         self.batch_size, self.use_tf, self.seed, self.gamma, self.epsilon, self.eps_min, self.eps_max, self.batch_size) = pickle.load(pkl_file)
        self.current_model.graph_net = keras.models.load_model("rpb_dqn_current_model_{}".format(id_str))
        self.target_model.graph_net = keras.models.load_model("rpb_dqn_target_model_{}".format(id_str))