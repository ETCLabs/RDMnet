# Getting Started                                                                {#getting_started}

For integrating the RDMnet library into your project, see \subpage building_and_integrating.

For an overview of how the RDMnet protocol works, see \ref how_it_works.

If you already know how RDMnet works and what you want to do with it, follow this guide to the API
that you would like to use:

| Role              | Description                                                                     | API guide                   |
|-------------------|---------------------------------------------------------------------------------|-----------------------------|
| RDMnet Controller | Configures devices by sending RDM commands over RDMnet.                         | \subpage using_controller   |
| RDMnet Device     | Accepts configuration by receiving and responding to RDM commands over RDMnet.  | \subpage using_device       |
| RDMnet Broker     | Routes messages between RDMnet Controllers, Devices and EPT clients.            | \subpage using_broker       |
| LLRP Manager      | Discovers and configures targets using \ref llrp.                               | \subpage using_llrp_manager |
| EPT Client        | Communicates through an RDMnet broker using one or more \ref ept sub-protocols. | \subpage using_ept_client   |

\htmlonly
<div style="display:none">
\endhtmlonly

\subpage handling_rdm_commands
\subpage global_init_and_destroy
\subpage data_ownership

\htmlonly
</div>
\endhtmlonly
